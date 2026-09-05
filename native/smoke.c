#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "process.h"

#ifdef _WIN32
#define PROG_TRUE "C:\\Windows\\System32\\hostname.exe"
#define PROG_CMD "C:\\Windows\\System32\\cmd.exe"
#define PROG_FINDSTR "C:\\Windows\\System32\\findstr.exe"
#else
#define PROG_TRUE "true"
#define PROG_ECHO "echo"
#define PROG_CAT "cat"
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

    printf("ok\n");
    return 0;
}
