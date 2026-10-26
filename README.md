# coil-process

Subprocess spawn, pipes, capture, and exit status for [Coil](https://github.com/ardax-corp/coil-lang). Package-native FFI (`extern "process"` + `Command` / `Child` with `fn drop()`). Not coil-lang HostInvoke.

This is where Coil runs programs: coil-lang has no `env::exec`. `exec(program, argv)` is the one-call form (inherit stdio, wait, exit code); `command` gives pipes, captured output, env and cwd.

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
