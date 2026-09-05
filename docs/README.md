# coil-process

Subprocess spawn with pipes and exit status for coil via `extern "process"` and userland `src/process.hy`.

## Package layout

| Path | Role |
|------|------|
| `src/process.hy` | `Command` / `Child` / `Output`, `fn drop()` |
| `native/process.c` | C ABI → `libprocess.so` |
| `tests/process.hy` | `coil test` suite |
| `examples/echo.hy` | Capture demo |

## Build native

```bash
make -C native
```

Produces `native/libprocess.so` (`.dylib` / `.dll` on other platforms).

## Test

```bash
make test
```

Needs `coil` on `PATH` (or `COIL=/path/to/coil`).

## See also

- [api.md](api.md)
- [consume.md](consume.md)
