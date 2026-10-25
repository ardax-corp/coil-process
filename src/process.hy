// Userland subprocess API — FFI shim over native/libprocess.
use io::IoError;

extern "process" {
    fn coil_proc_cmd_new(string program) -> int;
    fn coil_proc_cmd_free(int handle);
    fn coil_proc_cmd_arg(int handle, string arg) -> int;
    fn coil_proc_cmd_env(int handle, string key, string val) -> int;
    fn coil_proc_cmd_clear_env(int handle) -> int;
    fn coil_proc_cmd_cwd(int handle, string cwd) -> int;
    fn coil_proc_cmd_stdin(int handle, int mode) -> int;
    fn coil_proc_cmd_stdout(int handle, int mode) -> int;
    fn coil_proc_cmd_stderr(int handle, int mode) -> int;
    fn coil_proc_cmd_spawn(int handle) -> int;
    fn coil_proc_child_free(int handle);
    fn coil_proc_write_stdin(int handle, string data) -> int;
    fn coil_proc_close_stdin(int handle) -> int;
    fn coil_proc_read_stdout(int handle) -> string;
    fn coil_proc_read_stderr(int handle) -> string;
    fn coil_proc_wait(int handle) -> int;
    fn coil_proc_try_wait(int handle) -> int;
    fn coil_proc_kill(int handle) -> int;
    fn coil_proc_last_error() -> string;
    fn coil_proc_last_io_kind() -> int;
}

#[repr(int)]
enum Stdio {
    Inherit = 0,
    Pipe = 1,
    Capture = 2,
    Null = 3,
}

#[derive(Show, Eq)]
enum ProcessError {
    /// The program could not be started.
    Spawn,
    /// A pipe read or write failed; the OS error is kept.
    Io(IoError),
    /// Waiting for the child failed.
    Wait,
    /// The command or child was already used up or closed.
    Closed,
    /// A bad argument, environment entry, directory or stdio mode.
    Invalid,
}

class Command {
    pub handle: int,
}

class Child {
    pub handle: int,
}

class Output {
    pub status: int,
    pub stdout: string,
    pub stderr: string,
}

fn err_from_code(int code) -> ProcessError {
    if code == -1 {
        return ProcessError::Spawn;
    }
    if code == -2 {
        return ProcessError::Io(last_io_error());
    }
    if code == -3 {
        return ProcessError::Wait;
    }
    if code == -4 {
        return ProcessError::Closed;
    }
    return ProcessError::Invalid;
}

fn check_rc(int rc) -> Result<(), ProcessError> {
    if rc < 0 {
        raise err_from_code(rc);
    }
    return ();
}

/// The `IoError` for the native side's last OS failure (`COIL_PROC_IO_*`).
fn last_io_error() -> IoError {
    let kind = coil_proc_last_io_kind();
    if kind == 2 {
        return IoError::NotFound;
    }
    if kind == 3 {
        return IoError::PermissionDenied;
    }
    if kind == 4 {
        return IoError::AlreadyClosed;
    }
    if kind == 5 {
        return IoError::InvalidInput;
    }
    if kind == 6 {
        return IoError::WouldBlock;
    }
    if kind == 7 {
        return IoError::TimedOut;
    }
    return IoError::Other;
}

fn last_error() -> string {
    return coil_proc_last_error();
}

fn command(string program) -> Result<Command, ProcessError> {
    let h = coil_proc_cmd_new(program);
    if h == 0 {
        raise ProcessError::Spawn;
    }
    return new Command(h);
}

impl Child {
    fn drop() {
        if self.handle != 0 {
            coil_proc_child_free(self.handle);
            self.handle = 0;
        }
    }

    pub fn write_stdin(string data) -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_write_stdin(self.handle, data))?;
        return ();
    }

    pub fn close_stdin() -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_close_stdin(self.handle))?;
        return ();
    }

    pub fn read_stdout() -> Result<string, ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        return coil_proc_read_stdout(self.handle);
    }

    pub fn read_stderr() -> Result<string, ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        return coil_proc_read_stderr(self.handle);
    }

    pub fn wait() -> Result<int, ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        let rc = coil_proc_wait(self.handle);
        if rc < 0 {
            raise err_from_code(rc);
        }
        return rc;
    }

    pub fn try_wait() -> Result<Option<int>, ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        let rc = coil_proc_try_wait(self.handle);
        if rc == -6 {
            return Option::None;
        }
        if rc < 0 {
            raise err_from_code(rc);
        }
        return Option::Some(rc);
    }

    pub fn kill() -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_kill(self.handle))?;
        return ();
    }
}

impl Command {
    fn drop() {
        if self.handle != 0 {
            coil_proc_cmd_free(self.handle);
            self.handle = 0;
        }
    }

    pub fn arg(string value) -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_cmd_arg(self.handle, value))?;
        return ();
    }

    pub fn args(Vec<string> values) -> Result<(), ProcessError> {
        let i = 0;
        while i < values.len() {
            self.arg(values[i])?;
            i = i + 1;
        }
        return ();
    }

    pub fn env(string key, string val) -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_cmd_env(self.handle, key, val))?;
        return ();
    }

    pub fn env_clear() -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_cmd_clear_env(self.handle))?;
        return ();
    }

    pub fn cwd(string path) -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_cmd_cwd(self.handle, path))?;
        return ();
    }

    pub fn stdin(Stdio mode) -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_cmd_stdin(self.handle, mode))?;
        return ();
    }

    pub fn stdout(Stdio mode) -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_cmd_stdout(self.handle, mode))?;
        return ();
    }

    pub fn stderr(Stdio mode) -> Result<(), ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        check_rc(coil_proc_cmd_stderr(self.handle, mode))?;
        return ();
    }

    pub fn spawn() -> Result<Child, ProcessError> {
        if self.handle == 0 {
            raise ProcessError::Closed;
        }
        let h = coil_proc_cmd_spawn(self.handle);
        if h == 0 {
            raise ProcessError::Spawn;
        }
        self.handle = 0;
        return new Child(h);
    }

    pub fn status() -> Result<int, ProcessError> {
        let child = self.spawn()?;
        return child.wait()?;
    }

    pub fn output() -> Result<Output, ProcessError> {
        self.stdin(Stdio::Null)?;
        self.stdout(Stdio::Capture)?;
        self.stderr(Stdio::Capture)?;
        let child = self.spawn()?;
        let code = child.wait()?;
        let out = child.read_stdout()?;
        let err = child.read_stderr()?;
        return new Output(code, out, err);
    }
}
