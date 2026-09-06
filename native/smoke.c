#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "process.h"

#ifdef _WIN32
#define PROG_TRUE "C:\\Windows\\System32\\hostname.exe"
#define PROG_CMD "C:\\Windows\\System32\\cmd.exe"
#define PROG_FINDSTR "C:\\Windows\\System32\\findstr.exe"
#define PROG_PING "C:\\Windows\\System32\\ping.exe"
#else
#include <signal.h>
#define PROG_TRUE "true"
#define PROG_ECHO "echo"
#define PROG_CAT "cat"
#define PROG_SH "sh"
#define PROG_SLEEP "sleep"
#define PROG_FALSE "false"
#endif

static void fail(const char *msg) {
    char *err = coil_proc_last_error();
    fprintf(stderr, "%s: %s\n", msg, err ? err : "");
    free(err);
    exit(1);
}

static int64_t must_spawn(int64_t cmd) {
    int64_t child = coil_proc_cmd_spawn(cmd);
    if (!child) {
        fail("spawn");
    }
    return child;
}

int main(void) {
    int64_t cmd = coil_proc_cmd_new(PROG_TRUE);
    if (!cmd) {
        fail("cmd true");
    }
    coil_proc_cmd_stdout(cmd, COIL_PROC_NULL);
    coil_proc_cmd_stderr(cmd, COIL_PROC_NULL);
    int64_t child = must_spawn(cmd);
    int64_t code = coil_proc_wait(child);
    if (code != 0) {
        fprintf(stderr, "true exit %lld\n", (long long)code);
        return 1;
    }
    coil_proc_child_free(child);

#ifdef _WIN32
    cmd = coil_proc_cmd_new(PROG_CMD);
    if (!cmd) {
        fail("cmd echo");
    }
    coil_proc_cmd_arg(cmd, "/c");
    coil_proc_cmd_arg(cmd, "echo");
    coil_proc_cmd_arg(cmd, "hello");
#else
    cmd = coil_proc_cmd_new(PROG_ECHO);
    if (!cmd) {
        fail("cmd echo");
    }
    coil_proc_cmd_arg(cmd, "hello");
#endif
    coil_proc_cmd_stdout(cmd, COIL_PROC_CAPTURE);
    coil_proc_cmd_stderr(cmd, COIL_PROC_NULL);
    child = must_spawn(cmd);
    code = coil_proc_wait(child);
    if (code != 0) {
        fail("echo wait");
    }
    char *out = coil_proc_read_stdout(child);
    if (!out || strncmp(out, "hello", 5) != 0) {
        fprintf(stderr, "echo stdout=%s\n", out ? out : "(null)");
        free(out);
        return 1;
    }
    free(out);
    coil_proc_child_free(child);

#ifdef _WIN32
    cmd = coil_proc_cmd_new(PROG_FINDSTR);
    if (!cmd) {
        fail("cmd cat");
    }
    coil_proc_cmd_arg(cmd, "^");
#else
    cmd = coil_proc_cmd_new(PROG_CAT);
    if (!cmd) {
        fail("cmd cat");
    }
#endif
    coil_proc_cmd_stdin(cmd, COIL_PROC_PIPE);
    coil_proc_cmd_stdout(cmd, COIL_PROC_CAPTURE);
    coil_proc_cmd_stderr(cmd, COIL_PROC_NULL);
    child = must_spawn(cmd);
#ifdef _WIN32
    if (coil_proc_write_stdin(child, "ping-pong\n") < 0) {
#else
    if (coil_proc_write_stdin(child, "ping-pong") < 0) {
#endif
        fail("write");
    }
    if (coil_proc_close_stdin(child) != 0) {
        fail("close stdin");
    }
    code = coil_proc_wait(child);
    if (code != 0) {
        fail("cat wait");
    }
    out = coil_proc_read_stdout(child);
#ifdef _WIN32
    if (!out || strncmp(out, "ping-pong", 9) != 0) {
#else
    if (!out || strcmp(out, "ping-pong") != 0) {
#endif
        fprintf(stderr, "cat stdout=%s\n", out ? out : "(null)");
        free(out);
        return 1;
    }
    free(out);
    coil_proc_child_free(child);

    if (coil_proc_cmd_new("") != 0) {
        fprintf(stderr, "empty program should fail\n");
        return 1;
    }
    {
        char *err = coil_proc_last_error();
        if (!err || err[0] == '\0') {
            fprintf(stderr, "last_error empty after bad program\n");
            free(err);
            return 1;
        }
        free(err);
    }

    cmd = coil_proc_cmd_new(PROG_TRUE);
    if (!cmd) {
        fail("cmd for invalid stdin");
    }
    if (coil_proc_cmd_stdin(cmd, COIL_PROC_CAPTURE) != COIL_PROC_ERR_INVALID) {
        fail("stdin capture");
    }
    if (coil_proc_cmd_env(cmd, "", "x") != COIL_PROC_ERR_INVALID) {
        fail("empty env key");
    }
    if (coil_proc_cmd_env(cmd, "FOO=BAR", "x") != COIL_PROC_ERR_INVALID) {
        fail("env key equals");
    }
    coil_proc_cmd_free(cmd);

    if (coil_proc_cmd_arg(0, "x") != COIL_PROC_ERR_INVALID) {
        fail("arg on 0");
    }
    if (coil_proc_try_wait(0) != COIL_PROC_ERR_INVALID) {
        fail("try_wait on 0");
    }
    if (coil_proc_kill(0) != COIL_PROC_ERR_INVALID) {
        fail("kill on 0");
    }

    cmd = coil_proc_cmd_new("coil-process-no-such-bin");
    if (!cmd) {
        fail("cmd missing");
    }
    if (coil_proc_cmd_spawn(cmd) != 0) {
        fail("missing program spawned");
    }
    coil_proc_cmd_free(cmd);

    cmd = coil_proc_cmd_new(PROG_TRUE);
    if (!cmd) {
        fail("cmd write inherit");
    }
    coil_proc_cmd_stdout(cmd, COIL_PROC_NULL);
    coil_proc_cmd_stderr(cmd, COIL_PROC_NULL);
    child = must_spawn(cmd);
    if (coil_proc_write_stdin(child, "no-pipe") != COIL_PROC_ERR_CLOSED) {
        fail("write inherit");
    }
    code = coil_proc_wait(child);
    if (code != 0) {
        fail("true after write");
    }
    coil_proc_child_free(child);

#ifdef _WIN32
    cmd = coil_proc_cmd_new(PROG_CMD);
    if (!cmd) {
        fail("cmd exit 7");
    }
    coil_proc_cmd_arg(cmd, "/c");
    coil_proc_cmd_arg(cmd, "exit 7");
    coil_proc_cmd_stdout(cmd, COIL_PROC_NULL);
    coil_proc_cmd_stderr(cmd, COIL_PROC_NULL);
    child = must_spawn(cmd);
    code = coil_proc_wait(child);
    if (code != 7) {
        fprintf(stderr, "exit 7 got %lld\n", (long long)code);
        return 1;
    }
    if (coil_proc_wait(child) != 7) {
        fail("wait twice");
    }
    if (coil_proc_try_wait(child) != 7) {
        fail("try_wait after wait");
    }
    if (coil_proc_kill(child) != 0) {
        fail("kill after wait");
    }
    coil_proc_child_free(child);

    cmd = coil_proc_cmd_new(PROG_CMD);
    if (!cmd) {
        fail("cmd stderr");
    }
    coil_proc_cmd_arg(cmd, "/c");
    coil_proc_cmd_arg(cmd, "echo err 1>&2");
    coil_proc_cmd_stdout(cmd, COIL_PROC_CAPTURE);
    coil_proc_cmd_stderr(cmd, COIL_PROC_CAPTURE);
    child = must_spawn(cmd);
    code = coil_proc_wait(child);
    if (code != 0) {
        fail("stderr wait");
    }
    out = coil_proc_read_stdout(child);
    char *err = coil_proc_read_stderr(child);
    if (!err || strncmp(err, "err", 3) != 0) {
        fprintf(stderr, "stderr=%s stdout=%s\n", err ? err : "(null)", out ? out : "(null)");
        free(out);
        free(err);
        return 1;
    }
    free(out);
    free(err);
    coil_proc_child_free(child);

    cmd = coil_proc_cmd_new(PROG_PING);
    if (!cmd) {
        fail("cmd ping");
    }
    coil_proc_cmd_arg(cmd, "-n");
    coil_proc_cmd_arg(cmd, "40");
    coil_proc_cmd_arg(cmd, "127.0.0.1");
    coil_proc_cmd_stdin(cmd, COIL_PROC_NULL);
    coil_proc_cmd_stdout(cmd, COIL_PROC_NULL);
    coil_proc_cmd_stderr(cmd, COIL_PROC_NULL);
    child = must_spawn(cmd);
    if (coil_proc_try_wait(child) != COIL_PROC_PENDING) {
        fail("try_wait pending");
    }
    if (coil_proc_kill(child) != 0) {
        fail("kill ping");
    }
    code = coil_proc_wait(child);
    if (code == 0) {
        fprintf(stderr, "killed ping exited 0\n");
        return 1;
    }
    coil_proc_child_free(child);
#else
    cmd = coil_proc_cmd_new(PROG_FALSE);
    if (!cmd) {
        fail("cmd false");
    }
    coil_proc_cmd_stdout(cmd, COIL_PROC_NULL);
    coil_proc_cmd_stderr(cmd, COIL_PROC_NULL);
    child = must_spawn(cmd);
    code = coil_proc_wait(child);
    if (code != 1) {
        fprintf(stderr, "false exit %lld\n", (long long)code);
        return 1;
    }
    if (coil_proc_wait(child) != 1) {
        fail("wait twice");
    }
    if (coil_proc_try_wait(child) != 1) {
        fail("try_wait after wait");
    }
    if (coil_proc_kill(child) != 0) {
        fail("kill after wait");
    }
    coil_proc_child_free(child);

    cmd = coil_proc_cmd_new(PROG_SH);
    if (!cmd) {
        fail("cmd sh both");
    }
    coil_proc_cmd_arg(cmd, "-c");
    coil_proc_cmd_arg(cmd, "printf %s out; printf %s err >&2; exit 3");
    coil_proc_cmd_stdout(cmd, COIL_PROC_CAPTURE);
    coil_proc_cmd_stderr(cmd, COIL_PROC_CAPTURE);
    child = must_spawn(cmd);
    code = coil_proc_wait(child);
    if (code != 3) {
        fprintf(stderr, "sh exit %lld\n", (long long)code);
        return 1;
    }
    out = coil_proc_read_stdout(child);
    char *err = coil_proc_read_stderr(child);
    if (!out || strcmp(out, "out") != 0 || !err || strcmp(err, "err") != 0) {
        fprintf(stderr, "stdout=%s stderr=%s\n", out ? out : "(null)", err ? err : "(null)");
        free(out);
        free(err);
        return 1;
    }
    free(out);
    free(err);
    coil_proc_child_free(child);

    cmd = coil_proc_cmd_new(PROG_SLEEP);
    if (!cmd) {
        fail("cmd sleep");
    }
    coil_proc_cmd_arg(cmd, "30");
    coil_proc_cmd_stdin(cmd, COIL_PROC_NULL);
    coil_proc_cmd_stdout(cmd, COIL_PROC_NULL);
    coil_proc_cmd_stderr(cmd, COIL_PROC_NULL);
    child = must_spawn(cmd);
    if (coil_proc_try_wait(child) != COIL_PROC_PENDING) {
        fail("try_wait pending");
    }
    if (coil_proc_kill(child) != 0) {
        fail("kill sleep");
    }
    code = coil_proc_wait(child);
    if (code != 128 + SIGTERM) {
        fprintf(stderr, "killed sleep exit %lld\n", (long long)code);
        return 1;
    }
    coil_proc_child_free(child);
#endif

    printf("ok\n");
    return 0;
}
