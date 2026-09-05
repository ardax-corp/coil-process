use process::{command};
use io::{stdout, write};
use string::{format, to_bytes};

fn main() {
    let cmd = match command("echo") {
        Result::Ok(c) => c,
        Result::Err(_) => panic "command",
    };
    cmd.arg("hello")?;
    let out = match cmd.output() {
        Result::Ok(v) => v,
        Result::Err(_) => panic "output",
    };
    write(stdout(), to_bytes(format("%i,%s", out.status, out.stdout)));
}
