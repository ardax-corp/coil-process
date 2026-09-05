# coil-process

Subprocess spawn, pipes, capture, and exit status for [Coil](https://github.com/ardax-corp/coil-lang). Package-native FFI (`extern "process"` + `Command` / `Child` with `fn drop()`). Not coil-lang HostInvoke.

Coil already has wait-style `env::exec(program, argv) -> Result<int, EnvError>` (inherits stdio, no pipes, `--allow-exec`). Use this package when you need stdin/stdout/stderr pipes or captured output.

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
use process::{command, Stdio};

let out = command("echo")?.arg("hello")?.output()?;
// out.status == 0, out.stdout == "hello\n"
```

Run the demo:

```bash
coil --allow-dload process examples/echo.hy
```

## Docs

- [API](docs/api.md)
- [Consuming in a project](docs/consume.md)

## License

MIT — see [LICENSE](LICENSE).
