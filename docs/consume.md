# Consuming coil-process

Package name is `process`. Put this package's `src/` on `[module] roots` and `use process::{…}` resolves here. `extern "process"` in `src/process.hy` already loads the native library with `dload("process")`. Application code does not call `dload`.

Spool will own Coil-to-Coil deps once a public CLI can resolve git tags ([COI-219](https://linear.app/ardax/issue/COI-219)). Until then `{ git }` parses and the pin is `coil.lock`. Native `.so` / `.dylib` / `.dll` stay on `[ffi] search_paths` until [COI-60](https://linear.app/ardax/issue/COI-60).

`env::exec` remains the simpler Coil builtin: inherit stdio, no pipes, `--allow-exec`. This package is the Job 2 surface (spawn + pipes/capture + wait).

## Sibling checkout

This is the working path. Clone this repo next to your project. In the consumer `coil.toml`:

```toml
[module]
roots = ["./src", "../coil-process/src"]

[ffi]
search_paths = ["../coil-process/native"]
```

`[dependencies] process = { path = "../coil-process" }` is optional spool metadata. The compiler does not follow path deps for discovery. `roots` is what loads `src/process.hy`.

Build the native library from this package root:

```bash
make
# or: make -C native
```

That writes `native/libprocess.so` (`.dylib` / `.dll` on other platforms). `dload("process")` finds it on `search_paths`.

Then:

```coil
use process::{command, Stdio};

let cmd = command("echo")?;
cmd.arg("hello")?;
let out = cmd.output()?;
```

Function list and errors are in [api.md](api.md).

Run the consumer with `--allow-dload process`. `dload` also needs a pin or a trusted stem: spool passes it from `coil.lock` (`[[package.native]] sha256`) or a `trusted = true` dependency; running `coil` directly, pass `--dload-pin process=SHA256` or `--dload-trusted process` (`make test` pins the library it builds).

## Git dep and coil.lock

`{ git }` is the parseable form. `version` and `rev` are optional schema, stored only. They are not a resolved tag. Do not run `spool add`. There is no public spool CLI.

Parseable example:

```toml
[dependencies]
process = { git = "https://github.com/ardax-corp/coil-process.git" }

[module]
roots = ["./src", "./.spool/deps/process/src"]

[ffi]
search_paths = ["./.spool/deps/process/native"]
```

This repo has no tags until [COI-219](https://linear.app/ardax/issue/COI-219). The pin is `coil.lock` `rev` + `content_hash`. Omit `tag`. Use sibling checkout until spool materializes `.spool/deps`.

`coil.lock`:

```
# spool lockfile v1
[[package]]
name = 'process'
git = 'https://github.com/ardax-corp/coil-process.git'
rev = '<commit SHA>'
content_hash = '<tree SHA>'
```

`rev` is the commit. `content_hash` is that commit's git tree. After checkout:

```bash
git rev-parse HEAD
git rev-parse 'HEAD^{tree}'
```

The compiler does not read `coil.lock` for module discovery. Check out that `rev` and point `[module] roots` at its `src/`. Native libs stay on `[ffi] search_paths` until [COI-60](https://linear.app/ardax/issue/COI-60). Runtime `dload` still needs `--dload-pin process=SHA256` or `--dload-trusted process` (spool derives them from `coil.lock` / `trusted = true`).

## Lifecycle

- Prefer `let cmd = command("echo")?;` so `Command` is dropped if spawn never happens.
- After `spawn()`, the command handle is consumed (`handle == 0`). `Child.drop` closes pipes and reaps/kills a still-running child.
- Close pipe stdin (`close_stdin`) before `wait` when the child reads until EOF (`cat`).
