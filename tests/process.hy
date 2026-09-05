use process::{Command, Output, ProcessError, Stdio, command};

fn must_cmd(Result r) -> Command {
    return match r {
        Result::Ok(v) => v,
        Result::Err(_) => panic "expected Command",
    };
}

fn must_output(Result r) -> Output {
    return match r {
        Result::Ok(v) => v,
        Result::Err(_) => panic "expected Output",
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

test("true exits 0") {
    let cmd = must_cmd(command("true"));
    let code = match cmd.status() {
        Result::Ok(v) => v,
        Result::Err(_) => panic "status",
    };
    assert(code == 0)?;
}

test("echo captures stdout") {
    let cmd = must_cmd(command("echo"));
    cmd.arg("hello")?;
    let out = must_output(cmd.output());
    assert(out.status == 0)?;
    assert(out.stdout == "hello\n")?;
}

test("false exits 1") {
    let cmd = must_cmd(command("false"));
    let code = match cmd.status() {
        Result::Ok(v) => v,
        Result::Err(_) => panic "status",
    };
    assert(code == 1)?;
}

test("missing program is Spawn") {
    expect_spawn(must_cmd(command("coil-process-no-such-bin")).status());
}

test("cat pipe round-trip") {
    let cmd = must_cmd(command("cat"));
    cmd.stdin(Stdio::Pipe)?;
    cmd.stdout(Stdio::Capture)?;
    cmd.stderr(Stdio::Null)?;
    let child = match cmd.spawn() {
        Result::Ok(v) => v,
        Result::Err(_) => panic "spawn cat",
    };
    child.write_stdin("ping-pong")?;
    child.close_stdin()?;
    let code = match child.wait() {
        Result::Ok(v) => v,
        Result::Err(_) => panic "wait cat",
    };
    assert(code == 0)?;
    let text = match child.read_stdout() {
        Result::Ok(v) => v,
        Result::Err(_) => panic "read stdout",
    };
    assert(text == "ping-pong")?;
}

test("env override is visible") {
    let cmd = must_cmd(command("sh"));
    cmd.arg("-c")?;
    cmd.arg("printf %s \"$COIL_PROC_TEST\"")?;
    cmd.env("COIL_PROC_TEST", "from-env")?;
    let out = must_output(cmd.output());
    assert(out.status == 0)?;
    assert(out.stdout == "from-env")?;
}

test("cwd changes child directory") {
    let cmd = must_cmd(command("sh"));
    cmd.arg("-c")?;
    cmd.arg("pwd")?;
    cmd.cwd("/")?;
    let out = must_output(cmd.output());
    assert(out.status == 0)?;
    assert(out.stdout == "/\n")?;
}

test("drop then spawn is Closed") {
    let cmd = must_cmd(command("true"));
    cmd.drop();
    match cmd.status() {
        Result::Ok(_) => panic "expected Closed",
        Result::Err(e) => match e {
            ProcessError::Closed => {},
            default => panic "expected Closed",
        },
    };
    cmd.drop();
}
