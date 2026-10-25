// coil-process C ABI — OS spawn, pipes, wait. Not HostInvoke.
#pragma once

#include <stdint.h>

#define COIL_PROC_INHERIT 0
#define COIL_PROC_PIPE 1
#define COIL_PROC_CAPTURE 2
#define COIL_PROC_NULL 3

#define COIL_PROC_ERR_SPAWN (-1)
#define COIL_PROC_ERR_IO (-2)
#define COIL_PROC_ERR_WAIT (-3)
#define COIL_PROC_ERR_CLOSED (-4)
#define COIL_PROC_ERR_INVALID (-5)

/* `coil_proc_last_io_kind()`: what the last OS failure was, as `IoError`
 * names it. 0 when the last error was not an OS failure. */
#define COIL_PROC_IO_OTHER 1
#define COIL_PROC_IO_NOT_FOUND 2
#define COIL_PROC_IO_PERMISSION_DENIED 3
#define COIL_PROC_IO_ALREADY_CLOSED 4
#define COIL_PROC_IO_INVALID_INPUT 5
#define COIL_PROC_IO_WOULD_BLOCK 6
#define COIL_PROC_IO_TIMED_OUT 7
#define COIL_PROC_PENDING (-6)

int64_t coil_proc_cmd_new(const char *program);
void coil_proc_cmd_free(int64_t cmd);
int64_t coil_proc_cmd_arg(int64_t cmd, const char *arg);
int64_t coil_proc_cmd_env(int64_t cmd, const char *key, const char *val);
int64_t coil_proc_cmd_clear_env(int64_t cmd);
int64_t coil_proc_cmd_cwd(int64_t cmd, const char *cwd);
int64_t coil_proc_cmd_stdin(int64_t cmd, int64_t mode);
int64_t coil_proc_cmd_stdout(int64_t cmd, int64_t mode);
int64_t coil_proc_cmd_stderr(int64_t cmd, int64_t mode);
int64_t coil_proc_cmd_spawn(int64_t cmd);

void coil_proc_child_free(int64_t child);
int64_t coil_proc_write_stdin(int64_t child, const char *data);
int64_t coil_proc_close_stdin(int64_t child);
char *coil_proc_read_stdout(int64_t child);
char *coil_proc_read_stderr(int64_t child);
int64_t coil_proc_wait(int64_t child);
int64_t coil_proc_try_wait(int64_t child);
int64_t coil_proc_kill(int64_t child);
char *coil_proc_last_error(void);
int64_t coil_proc_last_io_kind(void);
