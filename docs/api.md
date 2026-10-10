# API

Module: `use process::{…};` (package name `process` from `coil.toml`).

OS spawn lives in this package's `native/` tree. It is not `env::exec` and not a coil-lang HostInvoke.

For a wait-only child that inherits stdio, Coil already has `env::exec(program, argv) -> Result<int, EnvError>` (`--allow-exec`). Use this package when you need pipes, capture, cwd, or a `Child` handle.

## Types

```coil
#[repr(int)]
enum Stdio { Inherit = 0, Pipe = 1, Capture = 2, Null = 3 }

enum ProcessError { Spawn, Io, Wait, Closed, Invalid }

class Command { handle: int }
class Child { handle: int }
class Output { status: int, stdout: string, stderr: string }
```

Handles are opaque native pointers cast to `int`. `Command` and `Child` run `coil_proc_*_free` from inherent `fn drop()`.

`Stdin` uses `Inherit | Pipe | Null`. `stdout` / `stderr` use `Inherit | Pipe | Capture` (`Null` is also accepted).

## Functions

| Function | Description |
|----------|-------------|
| `command(program)` | Build `Command` |
| `last_error()` | Last native error string (empty if none) |

`Command` builders return `Result<(), ProcessError>` (do not return `self`; a returned `Command` would run `drop` and free the handle):

| Method | Description |
|--------|-------------|
| `arg` / `args` | Extra argv after `program` |
| `env(key, val)` | Set or override one variable (inherits the rest) |
| `env_clear()` | Drop inherited env; only later `env` keys remain |
| `cwd(path)` | Child working directory |
| `stdin` / `stdout` / `stderr` | `Stdio` modes |
| `spawn()` | Start process → `Child` (consumes the command) |
| `status()` | Spawn + wait, inherit stdio, return exit code |
| `output()` | Null stdin, capture stdout/stderr, wait → `Output` |

`Child`:

| Method | Description |
|--------|-------------|
| `write_stdin` / `close_stdin` | Pipe stdin |
| `read_stdout` / `read_stderr` | Captured or drained pipe bytes as a string |
| `wait` / `try_wait` | Exit code (`try_wait` → `Option<int>`) |
| `kill` | SIGTERM / `TerminateProcess` |

Exit codes are the OS status (`128 + signal` on POSIX if signaled). `try_wait` returns `None` while the child is still running.

Text I/O is C-string based (no interior NUL) in v1.

## Errors

| Variant | When |
|---------|------|
| `Spawn` | Program missing, `fork`/`CreateProcess` failed, empty program |
| `Io` | Pipe read/write failed |
| `Wait` | `waitpid` / wait failed or kill failed |
| `Closed` | Use after `drop` or consumed `Command` |
| `Invalid` | Bad mode, env key, or handle |

## C ABI (FFI)

Declared in `extern "process"` inside `process.hy` (`coil_proc_*`). Names are not libc `system` / `execve` / `posix_spawn`, so `--allow-ffi-exec` is not required. Loading the library still needs `--allow-dload process` and `--dload-pin process=SHA256` or `--dload-trusted process` (spool passes them from `coil.lock` / `trusted = true`; see [consume.md](consume.md)).
