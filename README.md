# coil-process

> **Deprecated.** Coil now has a built-in [`process`](https://github.com/ardax-corp/coil-website/tree/main/src/content/reference/process) module ([coil-lang#2141](https://github.com/ardax-corp/coil-lang/pull/2141)). It covers `exec`, `Command` (args, env, cwd, stdio), `Child` (pipes as `io::Stream`s, `wait` / `try_wait` / `kill` / `signal`) and captured `output`, needs no native library, and is granted with `--allow-exec` (`[permissions] exec = true` in spool). This package receives no more changes and will be archived.
>
> Migrating: `exec(program, argv)` keeps its signature, so drop this dependency and its `[ffi]` entries and add the grant. `command(p)?` becomes `Command::new(p)`, and the setters no longer return a `Result`.

Subprocess spawn, pipes, capture, and exit status for [Coil](https://github.com/ardax-corp/coil-lang). Package-native FFI (`extern "process"` + `Command` / `Child` with `fn drop()`). Not coil-lang HostInvoke.

`exec(program, argv)` is the one-call form (inherit stdio, wait, exit code); `command` gives pipes, captured output, env and cwd.

## Requirements

- coil-lang with finalizers (inherent `fn drop()`)
- libffi (for coil FFI)
- A C11 toolchain (`cc`)

## Quick start

```bash
make          # native/libprocess.{so,dylib,dll}
make smoke    # C ABI smoke (true / echo / cat)
make test     # coil language harness (needs coil on PATH)
```

```coil
use process::{command};

let cmd = command("echo")?;
cmd.arg("hello")?;
let out = cmd.output()?;
// out.status == 0, out.stdout == "hello\n"
```

Run the demo:

```bash
coil --ffi-search-path native --allow-dload process --dload-trusted process examples/echo.hy
```

## Docs

- [API](docs/api.md)
- [Consuming in a project](docs/consume.md)

## License

MIT — see [LICENSE](LICENSE).
