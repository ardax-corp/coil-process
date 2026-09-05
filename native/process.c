// native/process.c — spawn / pipes / wait for coil-process.

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "process.h"

#ifdef _WIN32
extern char **environ;
#else
extern char **environ;
#endif

static char g_last_error[256];

static void set_error(const char *msg) {
    if (!msg) {
        g_last_error[0] = '\0';
        return;
    }
    snprintf(g_last_error, sizeof(g_last_error), "%s", msg);
}

#ifdef _WIN32
static void set_error_os(const char *prefix) {
    DWORD code = GetLastError();
    snprintf(g_last_error, sizeof(g_last_error), "%s: win32 %lu", prefix, (unsigned long)code);
}
#else
static void set_error_os(const char *prefix) {
    snprintf(g_last_error, sizeof(g_last_error), "%s: %s", prefix, strerror(errno));
}
#endif

static char *dup_cstr(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, n + 1);
    return out;
}

static char *empty_cstr(void) {
    char *out = (char *)malloc(1);
    if (!out) {
        return NULL;
    }
    out[0] = '\0';
    return out;
}

static int grow_ptrs(char ***list, size_t *cap, size_t need) {
    if (need <= *cap) {
        return 0;
    }
    size_t ncap = *cap ? *cap * 2 : 8;
    while (ncap < need) {
        ncap *= 2;
    }
    char **next = (char **)realloc(*list, ncap * sizeof(char *));
    if (!next) {
        return -1;
    }
    *list = next;
    *cap = ncap;
    return 0;
}

typedef struct {
    char *program;
    char **args;
    size_t argc;
    size_t args_cap;
    char **envs;
    size_t envc;
    size_t envs_cap;
    int inherit_env;
    char *cwd;
    int stdin_mode;
    int stdout_mode;
    int stderr_mode;
} CoilProcCmd;

#ifdef _WIN32
typedef struct {
    PROCESS_INFORMATION pi;
    HANDLE stdin_w;
    HANDLE stdout_r;
    HANDLE stderr_r;
    char *cap_out;
    size_t cap_out_len;
    char *cap_err;
    size_t cap_err_len;
    int started;
    int exited;
    int exit_code;
} CoilProcChild;
#else
typedef struct {
    pid_t pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    char *cap_out;
    size_t cap_out_len;
    char *cap_err;
    size_t cap_err_len;
    int exited;
    int exit_code;
} CoilProcChild;
#endif

static CoilProcCmd *cmd_from(int64_t handle) {
    if (handle == 0) {
        return NULL;
    }
    return (CoilProcCmd *)(intptr_t)handle;
}

static CoilProcChild *child_from(int64_t handle) {
    if (handle == 0) {
        return NULL;
    }
    return (CoilProcChild *)(intptr_t)handle;
}

static int valid_mode(int mode, int is_stdin) {
    if (mode == COIL_PROC_INHERIT || mode == COIL_PROC_PIPE || mode == COIL_PROC_NULL) {
        return 1;
    }
    if (!is_stdin && mode == COIL_PROC_CAPTURE) {
        return 1;
    }
    return 0;
}

int64_t coil_proc_cmd_new(const char *program) {
    set_error(NULL);
    if (!program || program[0] == '\0') {
        set_error("empty program");
        return 0;
    }
    CoilProcCmd *cmd = (CoilProcCmd *)calloc(1, sizeof(CoilProcCmd));
    if (!cmd) {
        set_error("oom");
        return 0;
    }
    cmd->program = dup_cstr(program);
    if (!cmd->program) {
        free(cmd);
        set_error("oom");
        return 0;
    }
    cmd->inherit_env = 1;
    cmd->stdin_mode = COIL_PROC_INHERIT;
    cmd->stdout_mode = COIL_PROC_INHERIT;
    cmd->stderr_mode = COIL_PROC_INHERIT;
    return (int64_t)(intptr_t)cmd;
}

void coil_proc_cmd_free(int64_t handle) {
    CoilProcCmd *cmd = cmd_from(handle);
    if (!cmd) {
        return;
    }
    free(cmd->program);
    for (size_t i = 0; i < cmd->argc; i++) {
        free(cmd->args[i]);
    }
    free(cmd->args);
    for (size_t i = 0; i < cmd->envc; i++) {
        free(cmd->envs[i]);
    }
    free(cmd->envs);
    free(cmd->cwd);
    free(cmd);
}

int64_t coil_proc_cmd_arg(int64_t handle, const char *arg) {
    CoilProcCmd *cmd = cmd_from(handle);
    if (!cmd || !arg) {
        set_error("invalid arg");
        return COIL_PROC_ERR_INVALID;
    }
    if (grow_ptrs(&cmd->args, &cmd->args_cap, cmd->argc + 1) != 0) {
        set_error("oom");
        return COIL_PROC_ERR_INVALID;
    }
    char *copy = dup_cstr(arg);
    if (!copy) {
        set_error("oom");
        return COIL_PROC_ERR_INVALID;
    }
    cmd->args[cmd->argc++] = copy;
    return 0;
}

static int env_key_len(const char *entry) {
    const char *eq = strchr(entry, '=');
    if (!eq) {
        return (int)strlen(entry);
    }
    return (int)(eq - entry);
}

int64_t coil_proc_cmd_env(int64_t handle, const char *key, const char *val) {
    CoilProcCmd *cmd = cmd_from(handle);
    if (!cmd || !key || key[0] == '\0' || strchr(key, '=') || !val) {
        set_error("invalid env");
        return COIL_PROC_ERR_INVALID;
    }
    size_t kn = strlen(key);
    size_t vn = strlen(val);
    char *entry = (char *)malloc(kn + vn + 2);
    if (!entry) {
        set_error("oom");
        return COIL_PROC_ERR_INVALID;
    }
    memcpy(entry, key, kn);
    entry[kn] = '=';
    memcpy(entry + kn + 1, val, vn + 1);
    for (size_t i = 0; i < cmd->envc; i++) {
        if (env_key_len(cmd->envs[i]) == (int)kn && strncmp(cmd->envs[i], key, kn) == 0) {
            free(cmd->envs[i]);
            cmd->envs[i] = entry;
            return 0;
        }
    }
    if (grow_ptrs(&cmd->envs, &cmd->envs_cap, cmd->envc + 1) != 0) {
        free(entry);
        set_error("oom");
        return COIL_PROC_ERR_INVALID;
    }
    cmd->envs[cmd->envc++] = entry;
    return 0;
}

int64_t coil_proc_cmd_clear_env(int64_t handle) {
    CoilProcCmd *cmd = cmd_from(handle);
    if (!cmd) {
        set_error("invalid cmd");
        return COIL_PROC_ERR_INVALID;
    }
    for (size_t i = 0; i < cmd->envc; i++) {
        free(cmd->envs[i]);
    }
    free(cmd->envs);
    cmd->envs = NULL;
    cmd->envc = 0;
    cmd->envs_cap = 0;
    cmd->inherit_env = 0;
    return 0;
}

int64_t coil_proc_cmd_cwd(int64_t handle, const char *cwd) {
    CoilProcCmd *cmd = cmd_from(handle);
    if (!cmd || !cwd) {
        set_error("invalid cwd");
        return COIL_PROC_ERR_INVALID;
    }
    char *copy = dup_cstr(cwd);
    if (!copy) {
        set_error("oom");
        return COIL_PROC_ERR_INVALID;
    }
    free(cmd->cwd);
    cmd->cwd = copy;
    return 0;
}

int64_t coil_proc_cmd_stdin(int64_t handle, int64_t mode) {
    CoilProcCmd *cmd = cmd_from(handle);
    if (!cmd || !valid_mode((int)mode, 1)) {
        set_error("invalid stdin mode");
        return COIL_PROC_ERR_INVALID;
    }
    cmd->stdin_mode = (int)mode;
    return 0;
}

int64_t coil_proc_cmd_stdout(int64_t handle, int64_t mode) {
    CoilProcCmd *cmd = cmd_from(handle);
    if (!cmd || !valid_mode((int)mode, 0)) {
        set_error("invalid stdout mode");
        return COIL_PROC_ERR_INVALID;
    }
    cmd->stdout_mode = (int)mode;
    return 0;
}

int64_t coil_proc_cmd_stderr(int64_t handle, int64_t mode) {
    CoilProcCmd *cmd = cmd_from(handle);
    if (!cmd || !valid_mode((int)mode, 0)) {
        set_error("invalid stderr mode");
        return COIL_PROC_ERR_INVALID;
    }
    cmd->stderr_mode = (int)mode;
    return 0;
}

static int append_bytes(char **buf, size_t *len, const char *src, size_t n) {
    char *next = (char *)realloc(*buf, *len + n + 1);
    if (!next) {
        return -1;
    }
    memcpy(next + *len, src, n);
    *len += n;
    next[*len] = '\0';
    *buf = next;
    return 0;
}

static char **build_argv(CoilProcCmd *cmd) {
    char **argv = (char **)calloc(cmd->argc + 2, sizeof(char *));
    if (!argv) {
        return NULL;
    }
    argv[0] = cmd->program;
    for (size_t i = 0; i < cmd->argc; i++) {
        argv[i + 1] = cmd->args[i];
    }
    return argv;
}

static int env_override_index(char **list, size_t n, const char *entry) {
    int kn = env_key_len(entry);
    for (size_t i = 0; i < n; i++) {
        if (env_key_len(list[i]) == kn && strncmp(list[i], entry, (size_t)kn) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static char **build_envp(CoilProcCmd *cmd) {
    if (cmd->inherit_env && cmd->envc == 0) {
        return NULL;
    }
    size_t base = 0;
    if (cmd->inherit_env && environ) {
        while (environ[base]) {
            base++;
        }
    }
    char **envp = (char **)calloc(base + cmd->envc + 1, sizeof(char *));
    if (!envp) {
        return (char **)(intptr_t)-1;
    }
    size_t n = 0;
    if (cmd->inherit_env && environ) {
        for (size_t i = 0; environ[i]; i++) {
            envp[n++] = environ[i];
        }
    }
    for (size_t i = 0; i < cmd->envc; i++) {
        int idx = env_override_index(envp, n, cmd->envs[i]);
        if (idx >= 0) {
            envp[idx] = cmd->envs[i];
        } else {
            envp[n++] = cmd->envs[i];
        }
    }
    envp[n] = NULL;
    return envp;
}

static const char *env_get(char **envp, const char *key) {
    size_t kn = strlen(key);
    if (envp) {
        for (size_t i = 0; envp[i]; i++) {
            if (strncmp(envp[i], key, kn) == 0 && envp[i][kn] == '=') {
                return envp[i] + kn + 1;
            }
        }
        return NULL;
    }
    return getenv(key);
}

#ifndef _WIN32
static int has_slash(const char *s) {
    return s && strchr(s, '/') != NULL;
}

static char *join_path(const char *dir, const char *name) {
    size_t a = strlen(dir);
    size_t b = strlen(name);
    int slash = a > 0 && dir[a - 1] == '/';
    char *out = (char *)malloc(a + b + (slash ? 1 : 2));
    if (!out) {
        return NULL;
    }
    memcpy(out, dir, a);
    size_t n = a;
    if (!slash) {
        out[n++] = '/';
    }
    memcpy(out + n, name, b + 1);
    return out;
}

static char *resolve_program(const char *program, char **envp) {
    if (has_slash(program)) {
        return dup_cstr(program);
    }
    const char *path = env_get(envp, "PATH");
    if (!path || path[0] == '\0') {
        path = "/usr/bin:/bin";
    }
    const char *p = path;
    while (*p) {
        const char *colon = strchr(p, ':');
        size_t n = colon ? (size_t)(colon - p) : strlen(p);
        char dir[4096];
        if (n >= sizeof(dir)) {
            n = sizeof(dir) - 1;
        }
        memcpy(dir, p, n);
        dir[n] = '\0';
        if (n == 0) {
            memcpy(dir, ".", 2);
        }
        char *cand = join_path(dir, program);
        if (cand && access(cand, X_OK) == 0) {
            return cand;
        }
        free(cand);
        if (!colon) {
            break;
        }
        p = colon + 1;
    }
    return NULL;
}

static int open_devnull(void) {
    return open("/dev/null", O_RDWR);
}

static void close_if(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

static int append_fd(int fd, char **buf, size_t *len) {
    char tmp[4096];
    for (;;) {
        ssize_t n = read(fd, tmp, sizeof(tmp));
        if (n > 0) {
            if (append_bytes(buf, len, tmp, (size_t)n) != 0) {
                return -1;
            }
            continue;
        }
        if (n == 0) {
            return 1;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
}

static int drain_child(CoilProcChild *ch, int block) {
    int out_open = ch->stdout_fd >= 0;
    int err_open = ch->stderr_fd >= 0;
    while (out_open || err_open) {
        struct pollfd fds[2];
        nfds_t n = 0;
        int out_i = -1;
        int err_i = -1;
        if (out_open) {
            out_i = (int)n;
            fds[n].fd = ch->stdout_fd;
            fds[n].events = POLLIN;
            n++;
        }
        if (err_open) {
            err_i = (int)n;
            fds[n].fd = ch->stderr_fd;
            fds[n].events = POLLIN;
            n++;
        }
        int pr = poll(fds, n, block ? -1 : 0);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_error_os("poll");
            return -1;
        }
        if (pr == 0) {
            return 0;
        }
        if (out_i >= 0 && (fds[out_i].revents & (POLLIN | POLLHUP | POLLERR))) {
            int rc = append_fd(ch->stdout_fd, &ch->cap_out, &ch->cap_out_len);
            if (rc < 0) {
                set_error_os("read stdout");
                return -1;
            }
            if (rc > 0) {
                close(ch->stdout_fd);
                ch->stdout_fd = -1;
                out_open = 0;
            }
        }
        if (err_i >= 0 && (fds[err_i].revents & (POLLIN | POLLHUP | POLLERR))) {
            int rc = append_fd(ch->stderr_fd, &ch->cap_err, &ch->cap_err_len);
            if (rc < 0) {
                set_error_os("read stderr");
                return -1;
            }
            if (rc > 0) {
                close(ch->stderr_fd);
                ch->stderr_fd = -1;
                err_open = 0;
            }
        }
    }
    return 0;
}

static int reap_child(CoilProcChild *ch, int block) {
    if (ch->exited) {
        return 0;
    }
    int status = 0;
    pid_t rc;
    do {
        rc = waitpid(ch->pid, &status, block ? 0 : WNOHANG);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        set_error_os("waitpid");
        return -1;
    }
    if (rc == 0) {
        return 1;
    }
    if (WIFEXITED(status)) {
        ch->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        ch->exit_code = 128 + WTERMSIG(status);
    } else {
        ch->exit_code = 1;
    }
    ch->exited = 1;
    return 0;
}

static CoilProcChild *spawn_posix(CoilProcCmd *cmd) {
    int in_p[2] = {-1, -1};
    int out_p[2] = {-1, -1};
    int err_p[2] = {-1, -1};
    if (cmd->stdin_mode == COIL_PROC_PIPE && pipe(in_p) != 0) {
        set_error_os("pipe stdin");
        return NULL;
    }
    if ((cmd->stdout_mode == COIL_PROC_PIPE || cmd->stdout_mode == COIL_PROC_CAPTURE) &&
        pipe(out_p) != 0) {
        set_error_os("pipe stdout");
        close_if(in_p[0]);
        close_if(in_p[1]);
        return NULL;
    }
    if ((cmd->stderr_mode == COIL_PROC_PIPE || cmd->stderr_mode == COIL_PROC_CAPTURE) &&
        pipe(err_p) != 0) {
        set_error_os("pipe stderr");
        close_if(in_p[0]);
        close_if(in_p[1]);
        close_if(out_p[0]);
        close_if(out_p[1]);
        return NULL;
    }

    char **argv = build_argv(cmd);
    char **envp = build_envp(cmd);
    if (!argv || envp == (char **)(intptr_t)-1) {
        set_error("oom");
        free(argv);
        if (envp != (char **)(intptr_t)-1) {
            free(envp);
        }
        close_if(in_p[0]);
        close_if(in_p[1]);
        close_if(out_p[0]);
        close_if(out_p[1]);
        close_if(err_p[0]);
        close_if(err_p[1]);
        return NULL;
    }
    char *resolved = resolve_program(cmd->program, envp);
    if (!resolved) {
        set_error("program not found");
        free(argv);
        free(envp);
        close_if(in_p[0]);
        close_if(in_p[1]);
        close_if(out_p[0]);
        close_if(out_p[1]);
        close_if(err_p[0]);
        close_if(err_p[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        set_error_os("fork");
        free(argv);
        free(envp);
        free(resolved);
        close_if(in_p[0]);
        close_if(in_p[1]);
        close_if(out_p[0]);
        close_if(out_p[1]);
        close_if(err_p[0]);
        close_if(err_p[1]);
        return NULL;
    }
    if (pid == 0) {
        if (cmd->cwd && chdir(cmd->cwd) != 0) {
            _exit(127);
        }
        if (cmd->stdin_mode == COIL_PROC_PIPE) {
            dup2(in_p[0], STDIN_FILENO);
        } else if (cmd->stdin_mode == COIL_PROC_NULL) {
            int n = open_devnull();
            if (n >= 0) {
                dup2(n, STDIN_FILENO);
                close(n);
            }
        }
        if (cmd->stdout_mode == COIL_PROC_PIPE || cmd->stdout_mode == COIL_PROC_CAPTURE) {
            dup2(out_p[1], STDOUT_FILENO);
        } else if (cmd->stdout_mode == COIL_PROC_NULL) {
            int n = open_devnull();
            if (n >= 0) {
                dup2(n, STDOUT_FILENO);
                close(n);
            }
        }
        if (cmd->stderr_mode == COIL_PROC_PIPE || cmd->stderr_mode == COIL_PROC_CAPTURE) {
            dup2(err_p[1], STDERR_FILENO);
        } else if (cmd->stderr_mode == COIL_PROC_NULL) {
            int n = open_devnull();
            if (n >= 0) {
                dup2(n, STDERR_FILENO);
                close(n);
            }
        }
        close_if(in_p[0]);
        close_if(in_p[1]);
        close_if(out_p[0]);
        close_if(out_p[1]);
        close_if(err_p[0]);
        close_if(err_p[1]);
        if (envp) {
            execve(resolved, argv, envp);
        } else {
            execv(resolved, argv);
        }
        _exit(127);
    }

    close_if(in_p[0]);
    close_if(out_p[1]);
    close_if(err_p[1]);
    free(argv);
    free(envp);
    free(resolved);

    CoilProcChild *ch = (CoilProcChild *)calloc(1, sizeof(CoilProcChild));
    if (!ch) {
        set_error("oom");
        close_if(in_p[1]);
        close_if(out_p[0]);
        close_if(err_p[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return NULL;
    }
    ch->pid = pid;
    ch->stdin_fd = in_p[1];
    ch->stdout_fd = out_p[0];
    ch->stderr_fd = err_p[0];
    if (ch->stdout_fd >= 0) {
        fcntl(ch->stdout_fd, F_SETFL, O_NONBLOCK);
    }
    if (ch->stderr_fd >= 0) {
        fcntl(ch->stderr_fd, F_SETFL, O_NONBLOCK);
    }
    return ch;
}
#endif

#ifdef _WIN32
static void close_handle(HANDLE *h) {
    if (h && *h && *h != INVALID_HANDLE_VALUE) {
        CloseHandle(*h);
        *h = NULL;
    }
}

static int win_append(HANDLE h, char **buf, size_t *len) {
    char tmp[4096];
    DWORD got = 0;
    if (!ReadFile(h, tmp, sizeof(tmp), &got, NULL)) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
            return 1;
        }
        return -1;
    }
    if (got == 0) {
        return 1;
    }
    if (append_bytes(buf, len, tmp, (size_t)got) != 0) {
        return -1;
    }
    return 0;
}

static int drain_child_win(CoilProcChild *ch, int block) {
    (void)block;
    if (ch->stdout_r) {
        for (;;) {
            int rc = win_append(ch->stdout_r, &ch->cap_out, &ch->cap_out_len);
            if (rc < 0) {
                set_error_os("read stdout");
                return -1;
            }
            if (rc > 0) {
                close_handle(&ch->stdout_r);
                break;
            }
        }
    }
    if (ch->stderr_r) {
        for (;;) {
            int rc = win_append(ch->stderr_r, &ch->cap_err, &ch->cap_err_len);
            if (rc < 0) {
                set_error_os("read stderr");
                return -1;
            }
            if (rc > 0) {
                close_handle(&ch->stderr_r);
                break;
            }
        }
    }
    return 0;
}

static char *quote_win(const char *s) {
    size_t extra = 2;
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            extra++;
        }
        extra++;
    }
    char *out = (char *)malloc(extra + 1);
    if (!out) {
        return NULL;
    }
    size_t n = 0;
    out[n++] = '"';
    for (const char *p = s; *p; p++) {
        if (*p == '"') {
            out[n++] = '\\';
        }
        out[n++] = *p;
    }
    out[n++] = '"';
    out[n] = '\0';
    return out;
}

static char *build_cmdline(CoilProcCmd *cmd) {
    char **argv = build_argv(cmd);
    if (!argv) {
        return NULL;
    }
    size_t total = 1;
    for (size_t i = 0; argv[i]; i++) {
        total += strlen(argv[i]) + 3;
    }
    char *line = (char *)malloc(total + 8);
    if (!line) {
        free(argv);
        return NULL;
    }
    line[0] = '\0';
    for (size_t i = 0; argv[i]; i++) {
        char *q = quote_win(argv[i]);
        if (!q) {
            free(argv);
            free(line);
            return NULL;
        }
        if (i > 0) {
            strcat(line, " ");
        }
        strcat(line, q);
        free(q);
    }
    free(argv);
    return line;
}

static char *build_env_block(CoilProcCmd *cmd) {
    char **envp = build_envp(cmd);
    if (envp == (char **)(intptr_t)-1) {
        return (char *)(intptr_t)-1;
    }
    if (!envp) {
        return NULL;
    }
    size_t total = 2;
    for (size_t i = 0; envp[i]; i++) {
        total += strlen(envp[i]) + 1;
    }
    char *block = (char *)malloc(total);
    if (!block) {
        free(envp);
        return (char *)(intptr_t)-1;
    }
    char *p = block;
    for (size_t i = 0; envp[i]; i++) {
        size_t n = strlen(envp[i]) + 1;
        memcpy(p, envp[i], n);
        p += n;
    }
    *p = '\0';
    free(envp);
    return block;
}

static CoilProcChild *spawn_win(CoilProcCmd *cmd) {
    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE in_r = NULL, in_w = NULL;
    HANDLE out_r = NULL, out_w = NULL;
    HANDLE err_r = NULL, err_w = NULL;
    HANDLE nul = INVALID_HANDLE_VALUE;

    if (cmd->stdin_mode == COIL_PROC_PIPE) {
        if (!CreatePipe(&in_r, &in_w, &sa, 0)) {
            set_error_os("pipe stdin");
            return NULL;
        }
        SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    }
    if (cmd->stdout_mode == COIL_PROC_PIPE || cmd->stdout_mode == COIL_PROC_CAPTURE) {
        if (!CreatePipe(&out_r, &out_w, &sa, 0)) {
            set_error_os("pipe stdout");
            close_handle(&in_r);
            close_handle(&in_w);
            return NULL;
        }
        SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    }
    if (cmd->stderr_mode == COIL_PROC_PIPE || cmd->stderr_mode == COIL_PROC_CAPTURE) {
        if (!CreatePipe(&err_r, &err_w, &sa, 0)) {
            set_error_os("pipe stderr");
            close_handle(&in_r);
            close_handle(&in_w);
            close_handle(&out_r);
            close_handle(&out_w);
            return NULL;
        }
        SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
    }
    if (cmd->stdin_mode == COIL_PROC_NULL || cmd->stdout_mode == COIL_PROC_NULL ||
        cmd->stderr_mode == COIL_PROC_NULL) {
        nul = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          &sa, OPEN_EXISTING, 0, NULL);
    }

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    if (cmd->stdin_mode == COIL_PROC_PIPE) {
        si.hStdInput = in_r;
    } else if (cmd->stdin_mode == COIL_PROC_NULL) {
        si.hStdInput = nul;
    } else {
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    if (cmd->stdout_mode == COIL_PROC_PIPE || cmd->stdout_mode == COIL_PROC_CAPTURE) {
        si.hStdOutput = out_w;
    } else if (cmd->stdout_mode == COIL_PROC_NULL) {
        si.hStdOutput = nul;
    } else {
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (cmd->stderr_mode == COIL_PROC_PIPE || cmd->stderr_mode == COIL_PROC_CAPTURE) {
        si.hStdError = err_w;
    } else if (cmd->stderr_mode == COIL_PROC_NULL) {
        si.hStdError = nul;
    } else {
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }

    char *cmdline = build_cmdline(cmd);
    char *env_block = build_env_block(cmd);
    if (!cmdline || env_block == (char *)(intptr_t)-1) {
        set_error("oom");
        free(cmdline);
        close_handle(&in_r);
        close_handle(&in_w);
        close_handle(&out_r);
        close_handle(&out_w);
        close_handle(&err_r);
        close_handle(&err_w);
        close_handle(&nul);
        return NULL;
    }

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    BOOL ok = CreateProcessA(
        NULL,
        cmdline,
        NULL,
        NULL,
        TRUE,
        0,
        env_block,
        cmd->cwd,
        &si,
        &pi);
    free(cmdline);
    free(env_block);
    close_handle(&in_r);
    close_handle(&out_w);
    close_handle(&err_w);
    close_handle(&nul);
    if (!ok) {
        set_error_os("CreateProcess");
        close_handle(&in_w);
        close_handle(&out_r);
        close_handle(&err_r);
        return NULL;
    }

    CoilProcChild *ch = (CoilProcChild *)calloc(1, sizeof(CoilProcChild));
    if (!ch) {
        set_error("oom");
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        close_handle(&in_w);
        close_handle(&out_r);
        close_handle(&err_r);
        return NULL;
    }
    ch->pi = pi;
    ch->stdin_w = in_w;
    ch->stdout_r = out_r;
    ch->stderr_r = err_r;
    ch->started = 1;
    return ch;
}

static int reap_child_win(CoilProcChild *ch, int block) {
    if (ch->exited) {
        return 0;
    }
    DWORD wait = WaitForSingleObject(ch->pi.hProcess, block ? INFINITE : 0);
    if (wait == WAIT_TIMEOUT) {
        return 1;
    }
    if (wait != WAIT_OBJECT_0) {
        set_error_os("WaitForSingleObject");
        return -1;
    }
    DWORD code = 1;
    if (!GetExitCodeProcess(ch->pi.hProcess, &code)) {
        set_error_os("GetExitCodeProcess");
        return -1;
    }
    ch->exit_code = (int)code;
    ch->exited = 1;
    return 0;
}
#endif

int64_t coil_proc_cmd_spawn(int64_t handle) {
    CoilProcCmd *cmd = cmd_from(handle);
    if (!cmd) {
        set_error("invalid cmd");
        return 0;
    }
#ifdef _WIN32
    CoilProcChild *ch = spawn_win(cmd);
#else
    CoilProcChild *ch = spawn_posix(cmd);
#endif
    if (!ch) {
        return 0;
    }
    coil_proc_cmd_free(handle);
    return (int64_t)(intptr_t)ch;
}

void coil_proc_child_free(int64_t handle) {
    CoilProcChild *ch = child_from(handle);
    if (!ch) {
        return;
    }
#ifdef _WIN32
    close_handle(&ch->stdin_w);
    close_handle(&ch->stdout_r);
    close_handle(&ch->stderr_r);
    if (ch->started && !ch->exited) {
        TerminateProcess(ch->pi.hProcess, 1);
        WaitForSingleObject(ch->pi.hProcess, INFINITE);
    }
    if (ch->started) {
        CloseHandle(ch->pi.hThread);
        CloseHandle(ch->pi.hProcess);
    }
#else
    close_if(ch->stdin_fd);
    close_if(ch->stdout_fd);
    close_if(ch->stderr_fd);
    if (!ch->exited && ch->pid > 0) {
        kill(ch->pid, SIGTERM);
        waitpid(ch->pid, NULL, 0);
    }
#endif
    free(ch->cap_out);
    free(ch->cap_err);
    free(ch);
}

int64_t coil_proc_write_stdin(int64_t handle, const char *data) {
    CoilProcChild *ch = child_from(handle);
    if (!ch || !data) {
        set_error("invalid write");
        return COIL_PROC_ERR_INVALID;
    }
#ifdef _WIN32
    if (!ch->stdin_w) {
        set_error("stdin closed");
        return COIL_PROC_ERR_CLOSED;
    }
    size_t n = strlen(data);
    size_t off = 0;
    while (off < n) {
        DWORD wrote = 0;
        if (!WriteFile(ch->stdin_w, data + off, (DWORD)(n - off), &wrote, NULL)) {
            set_error_os("WriteFile stdin");
            return COIL_PROC_ERR_IO;
        }
        off += wrote;
    }
#else
    if (ch->stdin_fd < 0) {
        set_error("stdin closed");
        return COIL_PROC_ERR_CLOSED;
    }
    size_t n = strlen(data);
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(ch->stdin_fd, data + off, n - off);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_error_os("write stdin");
            return COIL_PROC_ERR_IO;
        }
        off += (size_t)w;
    }
#endif
    return (int64_t)n;
}

int64_t coil_proc_close_stdin(int64_t handle) {
    CoilProcChild *ch = child_from(handle);
    if (!ch) {
        set_error("invalid child");
        return COIL_PROC_ERR_INVALID;
    }
#ifdef _WIN32
    close_handle(&ch->stdin_w);
#else
    close_if(ch->stdin_fd);
    ch->stdin_fd = -1;
#endif
    return 0;
}

static char *take_buf(char **buf, size_t *len) {
    if (!*buf) {
        return empty_cstr();
    }
    char *out = *buf;
    *buf = NULL;
    *len = 0;
    return out;
}

char *coil_proc_read_stdout(int64_t handle) {
    CoilProcChild *ch = child_from(handle);
    if (!ch) {
        set_error("invalid child");
        return empty_cstr();
    }
#ifdef _WIN32
    if (ch->stdout_r) {
        drain_child_win(ch, 1);
    }
#else
    if (ch->stdout_fd >= 0 || ch->stderr_fd >= 0) {
        drain_child(ch, ch->exited ? 1 : 0);
    }
#endif
    return take_buf(&ch->cap_out, &ch->cap_out_len);
}

char *coil_proc_read_stderr(int64_t handle) {
    CoilProcChild *ch = child_from(handle);
    if (!ch) {
        set_error("invalid child");
        return empty_cstr();
    }
#ifdef _WIN32
    if (ch->stderr_r) {
        drain_child_win(ch, 1);
    }
#else
    if (ch->stdout_fd >= 0 || ch->stderr_fd >= 0) {
        drain_child(ch, ch->exited ? 1 : 0);
    }
#endif
    return take_buf(&ch->cap_err, &ch->cap_err_len);
}

int64_t coil_proc_wait(int64_t handle) {
    CoilProcChild *ch = child_from(handle);
    if (!ch) {
        set_error("invalid child");
        return COIL_PROC_ERR_INVALID;
    }
#ifdef _WIN32
    close_handle(&ch->stdin_w);
    if (drain_child_win(ch, 1) != 0) {
        return COIL_PROC_ERR_IO;
    }
    if (reap_child_win(ch, 1) != 0) {
        return COIL_PROC_ERR_WAIT;
    }
#else
    close_if(ch->stdin_fd);
    ch->stdin_fd = -1;
    if (drain_child(ch, 1) != 0) {
        return COIL_PROC_ERR_IO;
    }
    if (reap_child(ch, 1) != 0) {
        return COIL_PROC_ERR_WAIT;
    }
#endif
    return (int64_t)ch->exit_code;
}

int64_t coil_proc_try_wait(int64_t handle) {
    CoilProcChild *ch = child_from(handle);
    if (!ch) {
        set_error("invalid child");
        return COIL_PROC_ERR_INVALID;
    }
#ifdef _WIN32
    if (ch->stdout_r || ch->stderr_r) {
        // Non-blocking drain is limited on Win32 anonymous pipes.
    }
    int rc = reap_child_win(ch, 0);
    if (rc < 0) {
        return COIL_PROC_ERR_WAIT;
    }
    if (rc > 0) {
        return COIL_PROC_PENDING;
    }
    drain_child_win(ch, 1);
#else
    if (ch->stdout_fd >= 0 || ch->stderr_fd >= 0) {
        if (drain_child(ch, 0) != 0) {
            return COIL_PROC_ERR_IO;
        }
    }
    int rc = reap_child(ch, 0);
    if (rc < 0) {
        return COIL_PROC_ERR_WAIT;
    }
    if (rc > 0) {
        return COIL_PROC_PENDING;
    }
    if (ch->stdout_fd >= 0 || ch->stderr_fd >= 0) {
        drain_child(ch, 1);
    }
#endif
    return (int64_t)ch->exit_code;
}

int64_t coil_proc_kill(int64_t handle) {
    CoilProcChild *ch = child_from(handle);
    if (!ch) {
        set_error("invalid child");
        return COIL_PROC_ERR_INVALID;
    }
#ifdef _WIN32
    if (!ch->started || ch->exited) {
        return 0;
    }
    if (!TerminateProcess(ch->pi.hProcess, 1)) {
        set_error_os("TerminateProcess");
        return COIL_PROC_ERR_WAIT;
    }
#else
    if (ch->exited || ch->pid <= 0) {
        return 0;
    }
    if (kill(ch->pid, SIGTERM) != 0 && errno != ESRCH) {
        set_error_os("kill");
        return COIL_PROC_ERR_WAIT;
    }
#endif
    return 0;
}

char *coil_proc_last_error(void) {
    return dup_cstr(g_last_error[0] ? g_last_error : "");
}
