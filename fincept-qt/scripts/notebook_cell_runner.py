"""notebook_cell_runner.py - executes one Fincept Notebook cell.

Usage:
    python notebook_cell_runner.py run --cancel-file PATH --timeout SECONDS
                                       [--cwd DIR] [--python INTERPRETER]

The cell source arrives on stdin (UTF-8). The cell's own stdout/stderr pass
straight through, line-buffered, so the terminal can stream them. Structured
events are printed to stdout as single lines starting with the marker
"\\x1eFNB " followed by compact JSON:

    {"type": "started", "python": "...", "version": "3.11.9"}
    {"type": "result", "text": "<repr of the cell's last expression>"}
    {"type": "error", "ename": "...", "evalue": "...", "traceback": [...]}
    {"type": "done", "status": "ok" | "error" | "cancelled" | "timeout", "elapsed_ms": N}

Cancellation is cooperative: when the cancel file appears (or the deadline
passes) the cell gets KeyboardInterrupt, exactly like Jupyter's "interrupt
kernel"; a cell that ignores it is hard-stopped after a short grace period.
--python runs the cell under another interpreter (e.g. the numpy1 venv); this
process then only relays its output and enforces the same cancel/deadline.

The exit code is 0 whenever the runner itself worked - a failing cell is
reported through the "done" event, not the exit code.
"""

import argparse
import ast
import builtins
import json
import linecache
import os
import subprocess
import sys
import threading
import time
import traceback

MARKER = "\x1eFNB "
GRACE_SECONDS = 3.0
POLL_SECONDS = 0.1
MAX_RESULT_CHARS = 100_000


class _Tracked:
    """Stream proxy that remembers the last character written, so the runner
    can end a partial last line before its own event lines."""

    def __init__(self, stream):
        self._s = stream
        self.last = "\n"

    def write(self, s):
        if s:
            self.last = s[-1]
        return self._s.write(s)

    def __getattr__(self, name):
        return getattr(self._s, name)


class _State:
    reason = None  # "cancelled" / "timeout" once the watcher fires
    finished = False


def _emit(event):
    out = sys.stdout
    if getattr(out, "last", "\n") != "\n":
        out.write("\n")
    out.write(MARKER + json.dumps(event, default=str) + "\n")
    out.flush()


def _watch(cancel_file, deadline, state, on_fire):
    while not state.finished:
        if cancel_file and os.path.exists(cancel_file):
            state.reason = "cancelled"
        elif deadline and time.monotonic() >= deadline:
            state.reason = "timeout"
        if state.reason:
            on_fire()
            return
        time.sleep(POLL_SECONDS)


def _cell_traceback(exc):
    """Traceback lines with the runner's own frames removed."""
    tb = exc.__traceback__
    while tb is not None and tb.tb_frame.f_code.co_filename != "<cell>":
        tb = tb.tb_next
    if isinstance(exc, SyntaxError):
        return traceback.format_exception_only(type(exc), exc)
    return traceback.format_exception(type(exc), exc, tb)


def _execute(code, state, timeout):
    linecache.cache["<cell>"] = (len(code), None, code.splitlines(True), "<cell>")
    namespace = {"__name__": "__main__", "__builtins__": builtins}
    try:
        tree = ast.parse(code, filename="<cell>", mode="exec")
        last = None
        if tree.body and isinstance(tree.body[-1], ast.Expr):
            last = ast.Expression(tree.body.pop().value)
        exec(compile(tree, "<cell>", "exec"), namespace)
        if last is not None:
            value = eval(compile(last, "<cell>", "eval"), namespace)
            if value is not None:
                text = repr(value)
                if len(text) > MAX_RESULT_CHARS:
                    text = text[:MAX_RESULT_CHARS] + "\n... [result truncated at %d characters]" % MAX_RESULT_CHARS
                _emit({"type": "result", "text": text})
        return "ok"
    except KeyboardInterrupt as exc:
        if state.reason == "timeout":
            _emit({"type": "error", "ename": "TimeoutError",
                   "evalue": "Cell exceeded the %d-second time limit" % timeout,
                   "traceback": _cell_traceback(exc)})
            return "timeout"
        _emit({"type": "error", "ename": "KeyboardInterrupt", "evalue": "Interrupted by user",
               "traceback": _cell_traceback(exc)})
        return "cancelled" if state.reason else "error"
    except SystemExit as exc:
        if exc.code in (None, 0):
            return "ok"
        _emit({"type": "error", "ename": "SystemExit", "evalue": str(exc.code), "traceback": []})
        return "error"
    except BaseException as exc:  # noqa: BLE001 - report every failure of user code
        _emit({"type": "error", "ename": type(exc).__name__, "evalue": str(exc),
               "traceback": _cell_traceback(exc)})
        return "error"


def _run_in_process(args, code):
    state = _State()
    deadline = time.monotonic() + args.timeout if args.timeout > 0 else 0

    def fire():
        import _thread
        _thread.interrupt_main()  # KeyboardInterrupt in the cell
        end = time.monotonic() + GRACE_SECONDS
        while time.monotonic() < end:
            if state.finished:
                return
            time.sleep(0.05)
        # The cell swallowed the interrupt or is stuck in native code.
        status = state.reason or "cancelled"
        if status == "timeout":
            _emit({"type": "error", "ename": "TimeoutError",
                   "evalue": "Cell exceeded the %d-second time limit and was stopped" % args.timeout,
                   "traceback": []})
        else:
            _emit({"type": "error", "ename": "KeyboardInterrupt", "evalue": "Stopped by user", "traceback": []})
        _emit({"type": "done", "status": status, "elapsed_ms": int((time.monotonic() - started) * 1000)})
        sys.stdout.flush()
        os._exit(0)

    started = time.monotonic()
    threading.Thread(target=_watch, args=(args.cancel_file, deadline, state, fire), daemon=True).start()
    status = _execute(code, state, args.timeout)
    state.finished = True
    for stream in (sys.stdout, sys.stderr):
        if getattr(stream, "last", "\n") != "\n":
            stream.write("\n")
        stream.flush()
    _emit({"type": "done", "status": status, "elapsed_ms": int((time.monotonic() - started) * 1000)})
    return 0


def _relay(src, dst):
    for line in iter(src.readline, b""):
        dst.buffer.write(line)
        dst.flush()


def _run_child(args, code):
    """Run the cell under another interpreter, relaying its output."""
    cmd = [args.python, os.path.abspath(__file__), "run", "--cancel-file", args.cancel_file or "",
           "--timeout", str(args.timeout), "--child"]
    if args.cwd:
        cmd += ["--cwd", args.cwd]
    flags = 0x08000000 if os.name == "nt" else 0  # CREATE_NO_WINDOW
    try:
        child = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 creationflags=flags)
    except OSError as exc:
        _emit({"type": "error", "ename": "KernelError", "evalue": "Could not start %s: %s" % (args.python, exc),
               "traceback": []})
        _emit({"type": "done", "status": "error", "elapsed_ms": 0})
        return 0
    threads = [threading.Thread(target=_relay, args=(child.stdout, sys.stdout), daemon=True),
               threading.Thread(target=_relay, args=(child.stderr, sys.stderr), daemon=True)]
    for t in threads:
        t.start()
    child.stdin.write(code.encode("utf-8"))
    child.stdin.close()
    # The child enforces cancel/deadline itself; this is the backstop.
    hard_deadline = time.monotonic() + args.timeout + GRACE_SECONDS + 5 if args.timeout > 0 else None
    cancel_seen = None
    while child.poll() is None:
        now = time.monotonic()
        if cancel_seen is None and args.cancel_file and os.path.exists(args.cancel_file):
            cancel_seen = now
        if (cancel_seen and now - cancel_seen > GRACE_SECONDS + 3) or (hard_deadline and now > hard_deadline):
            child.kill()
            break
        time.sleep(POLL_SECONDS)
    child.wait()
    for t in threads:
        t.join(timeout=2)
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["run"])
    parser.add_argument("--cancel-file", default="")
    parser.add_argument("--timeout", type=int, default=1800)
    parser.add_argument("--cwd", default="")
    parser.add_argument("--python", default="")
    parser.add_argument("--child", action="store_true")
    args = parser.parse_args()

    for name in ("stdout", "stderr"):
        stream = getattr(sys, name)
        try:
            stream.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
        except (AttributeError, ValueError):
            pass
        setattr(sys, name, _Tracked(stream))

    code = sys.stdin.buffer.read().decode("utf-8", "replace")

    if args.python and not args.child:
        same = os.path.normcase(os.path.realpath(args.python)) == os.path.normcase(os.path.realpath(sys.executable))
        if not same:
            _emit({"type": "started", "python": args.python, "version": ""})
            return _run_child(args, code)

    if args.cwd and os.path.isdir(args.cwd):
        os.chdir(args.cwd)
        sys.path.insert(0, args.cwd)
    # No GUI windows from a cell: plt.show() must not block a headless process.
    os.environ.setdefault("MPLBACKEND", "Agg")

    if not args.child:
        _emit({"type": "started", "python": sys.executable, "version": sys.version.split()[0]})
    return _run_in_process(args, code)


if __name__ == "__main__":
    sys.exit(main())
