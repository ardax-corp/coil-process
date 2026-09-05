use process::{command};
use io::{stdout, write};
use string::{format, to_bytes};

fn main() {
    let cmd = match command("echo") {
        Result::Ok(c) => c,
        Result::Err(_) => panic "command",
    };
    match cmd.arg("hello") {
        Result::Ok(_) => {},
        Result::Err(_) => panic "arg",
    };
    let out = match cmd.output() {
        Result::Ok(v) => v,
        Result::Err(_) => panic "output",
    };
    write(stdout(), to_bytes(format("%i,%s", out.status, out.stdout)));
}
