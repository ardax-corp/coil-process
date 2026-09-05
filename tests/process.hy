use process::{Command, Child, Output, ProcessError, Stdio, command};

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

fn must_ok(Result r) {
    match r {
        Result::Ok(_) => {},
        Result::Err(_) => panic "expected Ok",
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

test("true exits 0") {
    let cmd = must_cmd(command("true"));
    assert(must_int(cmd.status()) == 0)?;
}

test("echo captures stdout") {
    let cmd = must_cmd(command("echo"));
    must_unit(cmd.arg("hello"));
    let out = must_output(cmd.output());
    assert(out.status == 0)?;
    assert(out.stdout == "hello\n")?;
}

test("false exits 1") {
    let cmd = must_cmd(command("false"));
    assert(must_int(cmd.status()) == 1)?;
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
    assert(must_int(child.wait()) == 0)?;
    assert(must_string(child.read_stdout()) == "ping-pong")?;
}

test("env override is visible") {
    let cmd = must_cmd(command("sh"));
    must_unit(cmd.arg("-c"));
    must_unit(cmd.arg("printf %s \"$COIL_PROC_TEST\""));
    must_unit(cmd.env("COIL_PROC_TEST", "from-env"));
    let out = must_output(cmd.output());
    assert(out.status == 0)?;
    assert(out.stdout == "from-env")?;
}

test("cwd changes child directory") {
    let cmd = must_cmd(command("sh"));
    must_unit(cmd.arg("-c"));
    must_unit(cmd.arg("pwd"));
    must_unit(cmd.cwd("/"));
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
