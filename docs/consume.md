# Consuming coil-process

Package name is `process`. Put this package's `src/` on `--root` and `use process::{…}` resolves here. `extern "process"` in `src/process.hy` already loads the native library with `dload("process")`. Application code does not call `dload`.

coil-lang has no `env::exec`; this package is how Coil code runs programs. `exec(program, argv)` waits and returns the exit code; `command` adds pipes, capture, env and cwd.

## Sibling checkout

Clone this repo next to your project. coil reads no `coil.toml` and no `coil.lock`, so pass the root and the FFI search path as flags (below).

Build the native library from this package root:

```bash
make
# or: make -C native
```

That writes `native/libprocess.so` (`.dylib` / `.dll` on other platforms). `dload("process")` finds it through `--ffi-search-path`.

Then:

```coil
use process::{command, Stdio};

let cmd = command("echo")?;
cmd.arg("hello")?;
let out = cmd.output()?;
```

Function list and errors are in [api.md](api.md).

Run the consumer with `--allow-dload process` plus a pin or a trusted stem:

```bash
coil --root ../coil-process/src --ffi-search-path ../coil-process/native \
  --allow-dload process --dload-trusted process main.hy
```

`--dload-pin process=<sha256 of libprocess.so>` instead of `--dload-trusted process` pins the build (`make test` pins the library it builds).

## With spool

```bash
spool add process --git https://github.com/ardax-corp/coil-process.git --rev <commit SHA>
```

This repo has no tags, so pin a commit with `--rev`. spool records the dep, writes `coil.lock` (`rev` + `content_hash`), links `.spool/deps/process` to the checkout's `src/`, and passes `--root` for it. spool does not build `libprocess`; build it in a checkout (above) and grant it in the consumer `coil.toml`:

```toml
[ffi]
search_paths = ["../coil-process/native"]
allow = ["process"]

[dependencies]
process = { git = "https://github.com/ardax-corp/coil-process.git", rev = "<commit SHA>", trusted = true }
```

spool turns these into `--ffi-search-path`, `--allow-dload process` and `--dload-trusted process`. A `[[package.native]]` row with `sha256` under the `process` package in `coil.lock` gives `--dload-pin process=SHA256` instead of `trusted = true`.

## Lifecycle

- Prefer `let cmd = command("echo")?;` so `Command` is dropped if spawn never happens.
- After `spawn()`, the command handle is consumed (`handle == 0`). `Child.drop` closes pipes and reaps/kills a still-running child.
- Close pipe stdin (`close_stdin`) before `wait` when the child reads until EOF (`cat`).
