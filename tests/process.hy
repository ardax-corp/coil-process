use tests::assert;
use process::{Command, Child, Output, ProcessError, Stdio, command, last_error};

fn must_cmd(Result r) -> Command {
    return match r {
        Result::Ok(v) => v,
        Result::Err(_) => panic "expected Command",
    };
}

fn must_child(Result r) -> Child {
    return match r {
        Result::Ok(v) => v,
        Result::Err(_) => panic "expected Child",
    };
}

fn must_output(Result r) -> Output {
    return match r {
        Result::Ok(v) => v,
        Result::Err(_) => panic "expected Output",
    };
}

fn must_int(Result r) -> int {
    return match r {
        Result::Ok(v) => v,
        Result::Err(_) => panic "expected int",
    };
}

fn must_string(Result r) -> string {
    return match r {
        Result::Ok(v) => v,
        Result::Err(_) => panic "expected string",
    };
}

fn must_unit(Result r) {
    match r {
        Result::Ok(_) => {},
        Result::Err(_) => panic "expected Ok",
    };
}

fn expect_spawn(Result r) {
    match r {
        Result::Ok(_) => panic "expected Err",
        Result::Err(e) => match e {
            ProcessError::Spawn => {},
            default => panic "expected Spawn",
        },
    };
}

fn expect_closed(Result r) {
    match r {
        Result::Ok(_) => panic "expected Closed",
        Result::Err(e) => match e {
            ProcessError::Closed => {},
            default => panic "expected Closed",
        },
    };
}

fn expect_closed_unit(Result r) {
    match r {
        Result::Ok(_) => panic "expected Closed",
        Result::Err(e) => match e {
            ProcessError::Closed => {},
            default => panic "expected Closed",
        },
    };
}

fn expect_closed_opt(Result r) {
    match r {
        Result::Ok(_) => panic "expected Closed",
        Result::Err(e) => match e {
            ProcessError::Closed => {},
            default => panic "expected Closed",
        },
    };
}

fn expect_invalid(Result r) {
    match r {
        Result::Ok(_) => panic "expected Invalid",
        Result::Err(e) => match e {
            ProcessError::Invalid => {},
            default => panic "expected Invalid",
        },
    };
}

fn expect_none(Result r) {
    match r {
        Result::Ok(opt) => match opt {
            Option::None => {},
            Option::Some(_) => panic "expected None",
        },
        Result::Err(_) => panic "expected Ok(None)",
    };
}

fn expect_some_int(Result r) -> int {
    return match r {
        Result::Ok(opt) => match opt {
            Option::Some(v) => v,
            Option::None => panic "expected Some",
        },
        Result::Err(_) => panic "expected Ok(Some)",
    };
}

test("true exits 0") {
    let cmd = must_cmd(command("true"));
    assert!(must_int(cmd.status()) == 0);
}

test("echo captures stdout") {
    let cmd = must_cmd(command("echo"));
    must_unit(cmd.arg("hello"));
    let out = must_output(cmd.output());
    assert!(out.status == 0);
    assert!(out.stdout == "hello\n");
}

test("false exits 1") {
    let cmd = must_cmd(command("false"));
    assert!(must_int(cmd.status()) == 1);
}

test("missing program is Spawn") {
    expect_spawn(must_cmd(command("coil-process-no-such-bin")).status());
}

test("cat pipe round-trip") {
    let cmd = must_cmd(command("cat"));
    must_unit(cmd.stdin(Stdio::Pipe));
    must_unit(cmd.stdout(Stdio::Capture));
    must_unit(cmd.stderr(Stdio::Null));
    let child = must_child(cmd.spawn());
    must_unit(child.write_stdin("ping-pong"));
    must_unit(child.close_stdin());
    assert!(must_int(child.wait()) == 0);
    assert!(must_string(child.read_stdout()) == "ping-pong");
}

test("env override is visible") {
    let cmd = must_cmd(command("sh"));
    must_unit(cmd.arg("-c"));
    must_unit(cmd.arg("printf %s \"$COIL_PROC_TEST\""));
    must_unit(cmd.env("COIL_PROC_TEST", "from-env"));
    let out = must_output(cmd.output());
    assert!(out.status == 0);
    assert!(out.stdout == "from-env");
}

test("cwd changes child directory") {
    let cmd = must_cmd(command("sh"));
    must_unit(cmd.arg("-c"));
    must_unit(cmd.arg("pwd"));
    must_unit(cmd.cwd("/"));
    let out = must_output(cmd.output());
    assert!(out.status == 0);
    assert!(out.stdout == "/\n");
}

test("drop then spawn is Closed") {
    let cmd = must_cmd(command("true"));
    cmd.drop();
    expect_closed(cmd.status());
    cmd.drop();
}

test("empty program is Spawn") {
    match command("") {
        Result::Ok(_) => panic "expected Spawn",
        Result::Err(e) => match e {
            ProcessError::Spawn => {},
            default => panic "expected Spawn",
        },
    };
    assert!(last_error() != "");
}

test("spawn wait is status") {
    let cmd = must_cmd(command("true"));
    let child = must_child(cmd.spawn());
    assert!(must_int(child.wait()) == 0);
}

test("echo via spawn captures stdout only") {
    let cmd = must_cmd(command("echo"));
    must_unit(cmd.arg("only-out"));
    must_unit(cmd.stdout(Stdio::Capture));
    must_unit(cmd.stderr(Stdio::Null));
    let child = must_child(cmd.spawn());
    assert!(must_int(child.wait()) == 0);
    assert!(must_string(child.read_stdout()) == "only-out\n");
    assert!(must_string(child.read_stderr()) == "");
}

test("output captures stdout and stderr") {
    let cmd = must_cmd(command("sh"));
    must_unit(cmd.arg("-c"));
    must_unit(cmd.arg("printf %s out; printf %s err >&2; exit 3"));
    let out = must_output(cmd.output());
    assert!(out.status == 3);
    assert!(out.stdout == "out");
    assert!(out.stderr == "err");
}

test("stderr capture without stdout") {
    let cmd = must_cmd(command("sh"));
    must_unit(cmd.arg("-c"));
    must_unit(cmd.arg("printf %s e1 >&2"));
    must_unit(cmd.stdout(Stdio::Null));
    must_unit(cmd.stderr(Stdio::Capture));
    let child = must_child(cmd.spawn());
    assert!(must_int(child.wait()) == 0);
    assert!(must_string(child.read_stdout()) == "");
    assert!(must_string(child.read_stderr()) == "e1");
}

test("cat accepts two stdin writes") {
    let cmd = must_cmd(command("cat"));
    must_unit(cmd.stdin(Stdio::Pipe));
    must_unit(cmd.stdout(Stdio::Capture));
    must_unit(cmd.stderr(Stdio::Null));
    let child = must_child(cmd.spawn());
    must_unit(child.write_stdin("ping-"));
    must_unit(child.write_stdin("pong"));
    must_unit(child.close_stdin());
    assert!(must_int(child.wait()) == 0);
    assert!(must_string(child.read_stdout()) == "ping-pong");
}

test("empty stdin pipe yields empty stdout") {
    let cmd = must_cmd(command("cat"));
    must_unit(cmd.stdin(Stdio::Pipe));
    must_unit(cmd.stdout(Stdio::Capture));
    must_unit(cmd.stderr(Stdio::Null));
    let child = must_child(cmd.spawn());
    must_unit(child.close_stdin());
    assert!(must_int(child.wait()) == 0);
    assert!(must_string(child.read_stdout()) == "");
}

test("write after close_stdin is Closed") {
    let cmd = must_cmd(command("cat"));
    must_unit(cmd.stdin(Stdio::Pipe));
    must_unit(cmd.stdout(Stdio::Null));
    must_unit(cmd.stderr(Stdio::Null));
    let child = must_child(cmd.spawn());
    must_unit(child.close_stdin());
    expect_closed_unit(child.write_stdin("late"));
    assert!(must_int(child.wait()) == 0);
}

test("write without pipe is Closed") {
    let cmd = must_cmd(command("true"));
    let child = must_child(cmd.spawn());
    expect_closed_unit(child.write_stdin("no-pipe"));
    assert!(must_int(child.wait()) == 0);
}

test("env_clear drops inherited keys") {
    let cmd = must_cmd(command("/bin/sh"));
    must_unit(cmd.env("COIL_PROC_TEST", "should-clear"));
    must_unit(cmd.env_clear());
    must_unit(cmd.arg("-c"));
    must_unit(cmd.arg("printf %s \"${COIL_PROC_TEST-UNSET}\""));
    let out = must_output(cmd.output());
    assert!(out.status == 0);
    assert!(out.stdout == "UNSET");
}

test("env override replaces same key") {
    let cmd = must_cmd(command("sh"));
    must_unit(cmd.arg("-c"));
    must_unit(cmd.arg("printf %s \"$COIL_PROC_TEST\""));
    must_unit(cmd.env("COIL_PROC_TEST", "first"));
    must_unit(cmd.env("COIL_PROC_TEST", "second"));
    let out = must_output(cmd.output());
    assert!(out.status == 0);
    assert!(out.stdout == "second");
}

test("invalid env key is Invalid") {
    let cmd = must_cmd(command("true"));
    expect_invalid(cmd.env("", "x"));
    expect_invalid(cmd.env("FOO=BAR", "x"));
    assert!(must_int(cmd.status()) == 0);
}

test("stdin Capture is Invalid") {
    let cmd = must_cmd(command("true"));
    expect_invalid(cmd.stdin(Stdio::Capture));
}

test("bad cwd exits 127") {
    let cmd = must_cmd(command("true"));
    must_unit(cmd.cwd("/coil-process-no-such-cwd"));
    assert!(must_int(cmd.status()) == 127);
}

test("consumed command is Closed") {
    let cmd = must_cmd(command("true"));
    let child = must_child(cmd.spawn());
    expect_closed_unit(cmd.arg("late"));
    assert!(must_int(child.wait()) == 0);
}

test("dropped child is Closed") {
    let cmd = must_cmd(command("true"));
    let child = must_child(cmd.spawn());
    child.drop();
    expect_closed(child.wait());
    expect_closed_opt(child.try_wait());
    expect_closed_unit(child.kill());
}

test("wait is idempotent") {
    let cmd = must_cmd(command("false"));
    let child = must_child(cmd.spawn());
    assert!(must_int(child.wait()) == 1);
    assert!(must_int(child.wait()) == 1);
    assert!(expect_some_int(child.try_wait()) == 1);
    must_unit(child.kill());
}

test("try_wait is None until kill") {
    let cmd = must_cmd(command("sleep"));
    must_unit(cmd.arg("30"));
    must_unit(cmd.stdin(Stdio::Null));
    must_unit(cmd.stdout(Stdio::Null));
    must_unit(cmd.stderr(Stdio::Null));
    let child = must_child(cmd.spawn());
    expect_none(child.try_wait());
    must_unit(child.kill());
    let code = must_int(child.wait());
    assert!(code != 0);
}
