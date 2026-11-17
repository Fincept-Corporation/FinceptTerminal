"""notebook_cell_runner.py - executes Fincept Notebook cells.

Two modes.

1. One cell, one process (kept for compatibility):

    python notebook_cell_runner.py run --cancel-file PATH --timeout SECONDS
                                       [--cwd DIR] [--python INTERPRETER] [--json-stdin]

   The cell source arrives on stdin (UTF-8). With --json-stdin, stdin is instead
   {"prefix": ["<earlier cell>", ...], "code": "<cell>"}: the earlier cells are
   re-executed first, silently, to restore the variables the cell depends on. The
   cell's own stdout/stderr pass straight through, line-buffered. Cancellation is
   the cancel file (or the deadline): the cell gets KeyboardInterrupt and is
   hard-stopped after a short grace period. --python runs the cell under another
   interpreter; this process then only relays its output.

2. A persistent kernel (one long-lived process per open notebook):

    python -u notebook_cell_runner.py kernel [--cwd DIR]

   Commands arrive on stdin, one compact JSON object per line:

    {"op": "execute", "id": "<run id>", "code": "<cell source>", "timeout": SECONDS}
    {"op": "interrupt"}                       - KeyboardInterrupt in the running cell
    {"op": "variables"}                       - re-send the variables event
    {"op": "inspect", "id": "...", "name": "pd.DataFrame.merge"}
    {"op": "complete", "id": "...", "prefix": "pd.Dat"}
    {"op": "preview", "id": "...", "name": "df"}
    {"op": "style", "style": {"bg", "fg", "dim", "grid", "edge", "cycle": [...]}}
                                              - chart colours of the app's theme
    {"op": "shutdown"}                        - also: stdin closed

   The same chart style can come at start in FINCEPT_NB_MPL_STYLE (JSON). It is
   applied when the first figure is made, unless the cell styled matplotlib.

   One cell runs at a time; the namespace persists between cells (Jupyter
   semantics). Everything the cell prints is wrapped into events, so stdout
   carries nothing but event lines (text written straight to file descriptor 1
   by native code is the only exception; the reader treats it as stdout).
   input() raises EOFError: the kernel has no terminal.

Events (both modes) are single stdout lines: the marker "\\x1eFNB " then compact JSON.

    {"type": "ready", "python": "...", "version": "3.11.9", "pid": N, "cwd": "..."}   kernel
    {"type": "started", "python": "...", "version": "3.11.9"}                         run
    {"type": "start", "id": "..."}                                    kernel: cell began
    {"type": "stream", "id": "...", "name": "stdout"|"stderr", "text": "..."}       kernel
    {"type": "result", "text": "<repr of the cell's last expression>"}
    {"type": "display", "mime": "image/png", "data": "<base64>", "width": W, "height": H}
        - one per matplotlib figure: at plt.show() (kernel) and for figures still
          open when the cell ends
    {"type": "display", "mime": "application/x-table", "columns": [...], "dtypes": [...],
     "index": [...], "index_name": "...", "rows": [[...], ...], "total_rows": N,
     "total_columns": M}
        - a pandas DataFrame / Series / 1-2D numpy array as the last expression or
          passed to display(); first 1000 rows x 60 columns. dtypes per column:
          "number" | "integer" | "bool" | "datetime" | "string" | "object"
    {"type": "error", "ename": "...", "evalue": "...", "traceback": [...], "line": N}
        - line: 1-based line of the cell where it failed (0 = not in the cell)
    {"type": "done", "status": "ok" | "error" | "cancelled" | "timeout", "elapsed_ms": N,
     "truncated": bool, "kernel_lost": bool}
    {"type": "variables", "items": [{"name", "type", "shape", "preview"}], "total": N}   kernel
    {"type": "inspect" | "complete" | "preview", "id": "...", ...}                       kernel

In kernel mode every per-cell event also carries "id". The exit code is 0
whenever the runner itself worked - a failing cell is reported through "done".
"""

import argparse
import ast
import builtins
import json
import linecache
import os
import queue
import signal
import subprocess
import sys
import threading
import time
import traceback
import types

MARKER = "\x1eFNB "
GRACE_SECONDS = 3.0
POLL_SECONDS = 0.1
MAX_RESULT_CHARS = 100_000
MAX_TABLE_ROWS = 1000
MAX_TABLE_COLUMNS = 60
MAX_FIGURES = 10
MAX_OUTPUT_CHARS = 2 * 1024 * 1024  # per cell (kernel mode); the rest is dropped and flagged
MAX_VARIABLES = 300
MAX_DOC_CHARS = 6000
INLINE_BACKEND = "fincept_nb_inline"


# ── Values -> JSON ─────────────────────────────────────────────────────────────

def _json_cell(v):
    """A table cell as JSON: numbers stay numbers (NaN / inf -> null), the rest text."""
    try:
        import numpy as np  # noqa: PLC0415 - optional
        if isinstance(v, np.generic):
            v = v.item()
    except Exception:  # noqa: BLE001
        pass
    if v is None or isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        if isinstance(v, float) and (v != v or v in (float("inf"), float("-inf"))):
            return None
        return v
    return _label(v)


def _label(v):
    """Text for an index value / column name / non-numeric cell."""
    try:
        import datetime as _dt  # noqa: PLC0415
        if isinstance(v, _dt.datetime):
            if (v.hour, v.minute, v.second, v.microsecond) == (0, 0, 0, 0):
                return v.strftime("%Y-%m-%d")
            return v.isoformat(sep=" ")
        if isinstance(v, _dt.date):
            return v.isoformat()
    except Exception:  # noqa: BLE001
        pass
    if isinstance(v, tuple):
        return " / ".join(_label(x) for x in v)
    try:
        if v != v:  # NaT / NaN
            return ""
    except Exception:  # noqa: BLE001
        pass
    return str(v)


def _dtype_kind(dtype):
    kind = getattr(dtype, "kind", "O")
    return {"f": "number", "c": "number", "i": "integer", "u": "integer", "b": "bool",
            "M": "datetime", "m": "string", "U": "string", "S": "string"}.get(kind, "object")


def _table_event(value):
    """display event for a pandas DataFrame / Series or a 1-2D numpy array, else None."""
    pd = sys.modules.get("pandas")
    np = sys.modules.get("numpy")
    try:
        if np is not None and isinstance(value, np.ndarray) and value.ndim in (1, 2) and value.size > 0:
            if pd is None:
                return None
            value = pd.DataFrame(value)
        if pd is None:
            return None
        if isinstance(value, pd.Series):
            value = value.to_frame(name=value.name if value.name is not None else "value")
        if not isinstance(value, pd.DataFrame):
            return None
        head = value.iloc[:MAX_TABLE_ROWS, :MAX_TABLE_COLUMNS]
        index_name = head.index.name
        if index_name is None and getattr(head.index, "names", None) and any(n is not None for n in head.index.names):
            index_name = " / ".join("" if n is None else str(n) for n in head.index.names)
        return {
            "type": "display", "mime": "application/x-table",
            "columns": [_label(c) for c in head.columns],
            "dtypes": [_dtype_kind(t) for t in head.dtypes],
            "index": [_label(i) for i in head.index],
            "index_name": "" if index_name is None else str(index_name),
            "rows": [[_json_cell(v) for v in row] for row in head.itertuples(index=False, name=None)],
            "total_rows": int(len(value)),
            "total_columns": int(value.shape[1]),
        }
    except Exception:  # noqa: BLE001 - a table we cannot read stays a text result
        return None


def _figure_events(close=True):
    """One image/png display event per open matplotlib figure (then close them)."""
    plt = sys.modules.get("matplotlib.pyplot")
    if plt is None:
        return []
    try:
        nums = list(plt.get_fignums())
    except Exception:  # noqa: BLE001
        return []
    import base64  # noqa: PLC0415
    import io  # noqa: PLC0415
    events = []
    for n in nums[:MAX_FIGURES]:
        try:
            fig = plt.figure(n)
            buf = io.BytesIO()
            fig.savefig(buf, format="png", dpi=100, bbox_inches="tight")
            data = buf.getvalue()
            width = height = 0
            if data[:8] == b"\x89PNG\r\n\x1a\n" and len(data) >= 24:
                width = int.from_bytes(data[16:20], "big")
                height = int.from_bytes(data[20:24], "big")
            events.append({"type": "display", "mime": "image/png", "width": width, "height": height,
                           "data": base64.b64encode(data).decode("ascii")})
        except Exception:  # noqa: BLE001 - one bad figure must not fail the cell
            pass
    if close:
        try:
            plt.close("all")
        except Exception:  # noqa: BLE001
            pass
    return events


def _emit_figures():
    for ev in _figure_events():
        _emit(ev)


def _cell_line(exc):
    """1-based line of the cell where `exc` was raised, 0 when not in the cell."""
    if isinstance(exc, SyntaxError) and exc.filename == "<cell>":
        return int(exc.lineno or 0)
    line = 0
    tb = exc.__traceback__
    while tb is not None:
        if tb.tb_frame.f_code.co_filename == "<cell>":
            line = tb.tb_lineno
        tb = tb.tb_next
    return line


def _cell_traceback(exc):
    """Traceback lines with the runner's own frames removed."""
    tb = exc.__traceback__
    while tb is not None and tb.tb_frame.f_code.co_filename != "<cell>":
        tb = tb.tb_next
    if isinstance(exc, SyntaxError):
        return traceback.format_exception_only(type(exc), exc)
    return traceback.format_exception(type(exc), exc, tb)


def _error_event(exc, ename=None, evalue=None, tb_lines=None):
    return {"type": "error", "ename": ename or type(exc).__name__,
            "evalue": str(exc) if evalue is None else evalue,
            "traceback": _cell_traceback(exc) if tb_lines is None else tb_lines,
            "line": _cell_line(exc)}


# ── Event output ───────────────────────────────────────────────────────────────

class _Tracked:
    """Stream proxy that remembers the last character written, so the runner
    can end a partial last line before its own event lines ("run" mode)."""

    def __init__(self, stream):
        self._s = stream
        self.last = "\n"

    def write(self, s):
        if s:
            self.last = s[-1]
        return self._s.write(s)

    def __getattr__(self, name):
        return getattr(self._s, name)


_EMIT_LOCK = threading.Lock()
_PROTOCOL_OUT = None  # kernel mode: the real stdout, written only through _emit
_CURRENT_ID = None    # kernel mode: id of the cell running now (stamped on its events)
_KERNEL = None        # kernel mode: the kernel (its buffered cell output goes out first)


def _emit(event):
    if _PROTOCOL_OUT is not None:
        if _KERNEL is not None and event.get("type") != "stream":
            _KERNEL.flush_streams()  # keep printed text before what follows it

        if _CURRENT_ID is not None and "id" not in event and event.get("type") in (
                "result", "display", "error", "done", "stream", "start"):
            event = dict(event, id=_CURRENT_ID)
        line = MARKER + json.dumps(event, default=str) + "\n"
        with _EMIT_LOCK:
            try:
                _PROTOCOL_OUT.write(line)
                _PROTOCOL_OUT.flush()
            except (OSError, ValueError):
                pass
        return
    out = sys.stdout
    if getattr(out, "last", "\n") != "\n":
        out.write("\n")
    out.write(MARKER + json.dumps(event, default=str) + "\n")
    out.flush()


def _interrupt_main():
    """KeyboardInterrupt in the main thread, waking it from sleep / blocking I/O
    where the platform allows."""
    try:
        if os.name == "nt":
            # The C-level handler also sets the event time.sleep() waits on.
            signal.raise_signal(signal.SIGINT)
        else:
            signal.pthread_kill(threading.main_thread().ident, signal.SIGINT)
        return
    except Exception:  # noqa: BLE001
        pass
    import _thread  # noqa: PLC0415
    _thread.interrupt_main()


class _State:
    reason = None  # "cancelled" / "timeout" once the watcher fires
    finished = False


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


# ── Mode 1: one cell, one process ─────────────────────────────────────────────

class _PrefixError(Exception):
    """An earlier cell failed while being re-run to restore notebook state."""

    def __init__(self, index, exc):
        super().__init__(str(exc))
        self.index = index
        self.exc = exc


def _replay_prefix(prefix, namespace):
    """Re-run earlier cells in `namespace` with their output discarded."""
    saved = sys.stdout, sys.stderr
    sink = open(os.devnull, "w", encoding="utf-8")
    sys.stdout = sys.stderr = sink
    try:
        for i, src in enumerate(prefix):
            name = "<earlier cell %d>" % (i + 1)
            linecache.cache[name] = (len(src), None, src.splitlines(True), name)
            try:
                exec(compile(src, name, "exec"), namespace)
            except KeyboardInterrupt:
                raise
            except SystemExit as exc:
                if exc.code not in (None, 0):
                    raise _PrefixError(i, exc) from None
            except BaseException as exc:  # noqa: BLE001 - reported as the cell's error
                raise _PrefixError(i, exc) from None
    finally:
        sys.stdout, sys.stderr = saved
        sink.close()
        # Figures drawn by the replayed cells were shown when those cells ran.
        plt = sys.modules.get("matplotlib.pyplot")
        if plt is not None:
            try:
                plt.close("all")
            except Exception:  # noqa: BLE001
                pass


def _run_cell_body(code, namespace):
    """Compile and run `code` in `namespace`; emit the last expression's value."""
    linecache.cache["<cell>"] = (len(code), None, code.splitlines(True), "<cell>")
    tree = ast.parse(code, filename="<cell>", mode="exec")
    last = None
    if tree.body and isinstance(tree.body[-1], ast.Expr):
        last = ast.Expression(tree.body.pop().value)
    exec(compile(tree, "<cell>", "exec"), namespace)
    if last is not None:
        value = eval(compile(last, "<cell>", "eval"), namespace)
        _show_value(value)


def _show_value(value):
    """result (+ table) events for a value - the cell's last expression or display(x)."""
    # A Figure / Axes as the last expression is shown as the image, not "<Figure ...>".
    if value is None or type(value).__module__.startswith("matplotlib"):
        return
    try:
        text = repr(value)
    except Exception as exc:  # noqa: BLE001
        text = "<%s: repr failed: %s>" % (type(value).__name__, exc)
    if len(text) > MAX_RESULT_CHARS:
        text = text[:MAX_RESULT_CHARS] + "\n... [result truncated at %d characters]" % MAX_RESULT_CHARS
    _emit({"type": "result", "text": text})
    table = _table_event(value)
    if table is not None:
        _emit(table)


def _execute(code, state, timeout, prefix=()):
    namespace = {"__name__": "__main__", "__builtins__": builtins}
    try:
        if prefix:
            _replay_prefix(prefix, namespace)
        _run_cell_body(code, namespace)
        _emit_figures()
        return "ok"
    except KeyboardInterrupt as exc:
        if state.reason == "timeout":
            _emit(_error_event(exc, "TimeoutError", "Cell exceeded the %d-second time limit" % timeout))
            return "timeout"
        _emit(_error_event(exc, "KeyboardInterrupt", "Interrupted by user"))
        return "cancelled" if state.reason else "error"
    except SystemExit as exc:
        if exc.code in (None, 0):
            return "ok"
        _emit({"type": "error", "ename": "SystemExit", "evalue": str(exc.code), "traceback": [], "line": 0})
        return "error"
    except _PrefixError as perr:
        inner = perr.exc
        _emit({"type": "error", "ename": type(inner).__name__,
               "evalue": "Re-running earlier cell %d of this session (to restore its variables) failed: %s"
                         % (perr.index + 1, inner),
               "traceback": traceback.format_exception_only(type(inner), inner), "line": 0})
        return "error"
    except BaseException as exc:  # noqa: BLE001 - report every failure of user code
        _emit(_error_event(exc))
        return "error"


def _run_in_process(args, code, prefix=()):
    state = _State()
    deadline = time.monotonic() + args.timeout if args.timeout > 0 else 0

    def fire():
        _interrupt_main()  # KeyboardInterrupt in the cell
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
                   "traceback": [], "line": 0})
        else:
            _emit({"type": "error", "ename": "KeyboardInterrupt", "evalue": "Stopped by user", "traceback": [],
                   "line": 0})
        _emit({"type": "done", "status": status, "elapsed_ms": int((time.monotonic() - started) * 1000)})
        sys.stdout.flush()
        os._exit(0)

    started = time.monotonic()
    threading.Thread(target=_watch, args=(args.cancel_file, deadline, state, fire), daemon=True).start()
    status = _execute(code, state, args.timeout, prefix)
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


def _run_child(args, raw):
    """Run the cell under another interpreter, relaying its output. `raw` is
    the stdin payload exactly as received (forwarded with the same format flag)."""
    cmd = [args.python, os.path.abspath(__file__), "run", "--cancel-file", args.cancel_file or "",
           "--timeout", str(args.timeout), "--child"]
    if args.cwd:
        cmd += ["--cwd", args.cwd]
    if args.json_stdin:
        cmd += ["--json-stdin"]
    flags = 0x08000000 if os.name == "nt" else 0  # CREATE_NO_WINDOW
    try:
        child = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 creationflags=flags)
    except OSError as exc:
        _emit({"type": "error", "ename": "KernelError", "evalue": "Could not start %s: %s" % (args.python, exc),
               "traceback": [], "line": 0})
        _emit({"type": "done", "status": "error", "elapsed_ms": 0})
        return 0
    threads = [threading.Thread(target=_relay, args=(child.stdout, sys.stdout), daemon=True),
               threading.Thread(target=_relay, args=(child.stderr, sys.stderr), daemon=True)]
    for t in threads:
        t.start()
    child.stdin.write(raw)
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


# ── Mode 2: persistent kernel ──────────────────────────────────────────────────

class _CellStream:
    """sys.stdout / sys.stderr while a kernel cell runs: text is collected by the
    kernel (one buffer for both, so their order is kept) and sent as "stream"
    events every 100 ms, when 16 KB are pending, before a display and at the
    end of the cell."""

    encoding = "utf-8"
    errors = "replace"

    def __init__(self, name, kernel, fd):
        self.name = name
        self._kernel = kernel
        self._fd = fd

    def write(self, s):
        if not isinstance(s, str):
            s = str(s)
        if s:
            self._kernel.buffer_text(self.name, s)
        return len(s)

    def writelines(self, lines):
        for line in lines:
            self.write(line)

    def flush(self):
        pass  # sent on the flusher's next tick at the latest

    def isatty(self):
        return False

    def readable(self):
        return False

    def writable(self):
        return True

    def fileno(self):
        return self._fd

    @property
    def closed(self):
        return False


class _NoInput:
    """sys.stdin of a kernel cell: the kernel has no terminal."""

    encoding = "utf-8"

    def read(self, *_):
        return ""

    def readline(self, *_):
        return ""

    def readlines(self, *_):
        return []

    def isatty(self):
        return False

    def readable(self):
        return True

    def fileno(self):
        # Descriptor 0 is the null device in the kernel (_private_command_stream), so a
        # child started with stdin=sys.stdin (fincept_connector does) gets an empty input.
        return 0

    def __iter__(self):
        return iter(())

    @property
    def closed(self):
        return False


def _install_inline_backend(kernel):
    """A matplotlib backend module whose show() sends the open figures to the
    notebook (like Jupyter's inline backend). Registered in sys.modules so
    MPLBACKEND=module://fincept_nb_inline resolves without a file on disk."""
    mod = types.ModuleType(INLINE_BACKEND)

    def show(*_args, **_kwargs):
        for ev in _figure_events():
            kernel.emit_display(ev)

    def __getattr__(name):  # noqa: N807 - module-level getattr (PEP 562)
        # Resolved on first use (matplotlib is not imported until the cell does)
        # and stored, so matplotlib's vars(backend_module) sees them afterwards.
        if name == "FigureCanvas":
            from matplotlib.backends.backend_agg import FigureCanvasAgg as value  # noqa: PLC0415
            kernel.apply_mpl_style()  # the first figure is about to be made
        elif name == "FigureManager":
            from matplotlib.backend_bases import FigureManagerBase as value  # noqa: PLC0415
        else:
            raise AttributeError(name)
        setattr(mod, name, value)
        return value

    mod.show = show
    mod.__getattr__ = __getattr__
    sys.modules[INLINE_BACKEND] = mod
    os.environ["MPLBACKEND"] = "module://" + INLINE_BACKEND


def _short_repr(value, limit=120):
    import reprlib  # noqa: PLC0415
    r = reprlib.Repr()
    r.maxstring = limit
    r.maxother = limit
    r.maxlist = r.maxtuple = r.maxset = r.maxfrozenset = r.maxdeque = r.maxarray = 8
    r.maxdict = 6
    r.maxlevel = 2
    try:
        text = r.repr(value)
    except Exception:  # noqa: BLE001
        text = "<%s>" % type(value).__name__
    text = " ".join(text.split())
    return text if len(text) <= limit else text[:limit - 1] + "…"


def _var_info(name, value):
    """(type, shape, preview) of one variable for the Variables pane."""
    tname = type(value).__name__
    pd = sys.modules.get("pandas")
    np = sys.modules.get("numpy")
    shape = ""
    preview = None
    if pd is not None and isinstance(value, pd.DataFrame):
        shape = "%d × %d" % value.shape
        cols = [str(c) for c in value.columns[:8]]
        preview = ", ".join(cols) + (", …" if value.shape[1] > 8 else "")
    elif pd is not None and isinstance(value, pd.Series):
        shape = "%d" % len(value)
        preview = "dtype %s" % value.dtype
    elif np is not None and isinstance(value, np.ndarray):
        shape = " × ".join(str(d) for d in value.shape) or "scalar"
        preview = "dtype %s" % value.dtype
    elif isinstance(value, (list, tuple, dict, set, frozenset, str, bytes)):
        shape = "%d" % len(value)
    if preview is None:
        preview = _short_repr(value)
    return tname, shape, preview


class _Kernel:
    HIDDEN = {"__name__", "__builtins__", "__doc__", "__loader__", "__spec__", "__package__", "display",
              "In", "Out", "exit", "quit", "get_ipython"}

    def __init__(self, cwd, command_stream):
        self.cwd = cwd
        self.ns = {"__name__": "__main__", "__builtins__": builtins}
        self.ns["display"] = self._display
        self.commands = queue.Queue()
        self.running = False
        self.state = None
        self.out_chars = 0
        self.truncated = False
        self.streams = {}
        self._real_stdin = command_stream
        self._pending = []  # [[name, text], ...] not sent yet, in write order
        self._pending_chars = 0
        self._out_lock = threading.Lock()
        # Chart colours of the terminal's theme (FINCEPT_NB_MPL_STYLE / the "style" op):
        # {"bg", "fg", "dim", "grid", "edge", "cycle": [...]}.
        self.mpl_style = None
        self.mpl_styled = False
        try:
            self.mpl_style = json.loads(os.environ.get("FINCEPT_NB_MPL_STYLE", "") or "null")
        except ValueError:
            self.mpl_style = None

    def apply_mpl_style(self, force=False):
        """Theme matplotlib like the terminal - unless the cell styled it itself."""
        style = self.mpl_style
        mpl = sys.modules.get("matplotlib")
        if not isinstance(style, dict) or mpl is None:
            return
        try:
            rc = mpl.rcParams
            if not force and not self.mpl_styled and str(rc["axes.facecolor"]).lower() not in ("white", "w", "#ffffff"):
                return
            bg, fg, dim = style.get("bg"), style.get("fg"), style.get("dim")
            grid, edge = style.get("grid"), style.get("edge")
            rc.update({
                "figure.facecolor": bg, "savefig.facecolor": bg, "axes.facecolor": bg,
                "axes.edgecolor": edge, "axes.labelcolor": dim, "axes.titlecolor": fg, "text.color": fg,
                "xtick.color": dim, "ytick.color": dim, "xtick.labelcolor": dim, "ytick.labelcolor": dim,
                "grid.color": grid, "legend.facecolor": bg, "legend.edgecolor": edge, "legend.labelcolor": fg,
                "axes.grid": True, "grid.linewidth": 0.6, "axes.spines.top": False, "axes.spines.right": False,
            })
            cycle = [c for c in style.get("cycle", []) if isinstance(c, str)]
            if cycle:
                from cycler import cycler  # noqa: PLC0415
                rc["axes.prop_cycle"] = cycler(color=cycle)
            self.mpl_styled = True
        except Exception:  # noqa: BLE001 - a style problem must never fail a cell
            pass

    # output ------------------------------------------------------------------
    def buffer_text(self, name, text):
        flush_now = False
        with self._out_lock:
            if self.out_chars >= MAX_OUTPUT_CHARS:
                self.truncated = True
                return
            room = MAX_OUTPUT_CHARS - self.out_chars
            if len(text) > room:
                text = text[:room]
                self.truncated = True
            self.out_chars += len(text)
            if self._pending and self._pending[-1][0] == name:
                self._pending[-1][1] += text
            else:
                self._pending.append([name, text])
            self._pending_chars += len(text)
            flush_now = self._pending_chars > 16384
        if flush_now:
            self.flush_streams()

    def flush_streams(self):
        with self._out_lock:
            chunks, self._pending, self._pending_chars = self._pending, [], 0
        for name, text in chunks:
            _emit({"type": "stream", "name": name, "text": text})

    def emit_display(self, event):
        self.flush_streams()
        _emit(event)

    def _display(self, *objs, **_kwargs):
        """display(obj, ...) - show values (tables, figures, text) in the cell's output."""
        self.flush_streams()
        for obj in objs:
            if type(obj).__module__.startswith("matplotlib"):
                for ev in _figure_events():
                    _emit(ev)
                continue
            _show_value(obj)

    # commands ----------------------------------------------------------------
    def reader(self):
        stdin = self._real_stdin
        while True:
            try:
                line = stdin.readline()
            except Exception:  # noqa: BLE001
                line = ""
            if not line:
                self.commands.put({"op": "shutdown"})
                if self.running:
                    self.interrupt("cancelled")
                return
            line = line.strip()
            if not line:
                continue
            try:
                cmd = json.loads(line)
            except ValueError:
                continue
            if not isinstance(cmd, dict):
                continue
            if cmd.get("op") == "interrupt":
                self.interrupt("cancelled")
            else:
                self.commands.put(cmd)

    def interrupt(self, reason):
        st = self.state
        if not self.running or st is None or st.finished or st.reason:
            return
        st.reason = reason
        _interrupt_main()
        threading.Thread(target=self._grace, args=(st,), daemon=True).start()

    def _grace(self, st):
        end = time.monotonic() + GRACE_SECONDS
        while time.monotonic() < end:
            if st.finished:
                return
            time.sleep(0.05)
        # The cell swallowed the interrupt or is stuck in native code: the kernel
        # cannot get its main thread back. Report and exit; the app restarts it.
        status = st.reason or "cancelled"
        self.flush_streams()
        if status == "timeout":
            _emit({"type": "error", "ename": "TimeoutError",
                   "evalue": "The cell exceeded its time limit and did not stop; the kernel was restarted "
                             "and its variables were cleared", "traceback": [], "line": 0})
        else:
            _emit({"type": "error", "ename": "KeyboardInterrupt",
                   "evalue": "The cell did not stop when interrupted; the kernel was restarted and its "
                             "variables were cleared", "traceback": [], "line": 0})
        _emit({"type": "done", "status": status, "elapsed_ms": int((time.monotonic() - st.started) * 1000),
               "truncated": self.truncated, "kernel_lost": True})
        os._exit(0)

    def flusher(self):
        while True:
            time.sleep(0.1)
            if self.running:
                self.flush_streams()

    def watchdog(self, st, deadline):
        while not st.finished:
            if deadline and time.monotonic() >= deadline:
                self.interrupt("timeout")
                return
            time.sleep(POLL_SECONDS)

    def execute(self, cmd):
        global _CURRENT_ID  # noqa: PLW0603
        code = str(cmd.get("code", ""))
        timeout = int(cmd.get("timeout", 0) or 0)
        st = _State()
        st.started = time.monotonic()
        self.state = st
        self.out_chars = 0
        self.truncated = False
        _CURRENT_ID = str(cmd.get("id", ""))
        self.running = True
        _emit({"type": "start"})
        if timeout > 0:
            threading.Thread(target=self.watchdog, args=(st, st.started + timeout), daemon=True).start()
        status = "ok"
        try:
            try:
                _run_cell_body(code, self.ns)
            finally:
                self.flush_streams()
            for ev in _figure_events():
                _emit(ev)
        except KeyboardInterrupt as exc:
            self.flush_streams()
            if st.reason == "timeout":
                _emit(_error_event(exc, "TimeoutError", "Cell exceeded the %d-second time limit" % timeout))
                status = "timeout"
            else:
                _emit(_error_event(exc, "KeyboardInterrupt", "Interrupted by user"))
                status = "cancelled"
        except SystemExit as exc:
            if exc.code not in (None, 0):
                _emit({"type": "error", "ename": "SystemExit", "evalue": str(exc.code), "traceback": [],
                       "line": _cell_line(exc)})
                status = "error"
        except BaseException as exc:  # noqa: BLE001 - report every failure of user code
            self.flush_streams()
            _emit(_error_event(exc))
            status = "error"
        finally:
            st.finished = True
        # An interrupt sent just as the cell ended can still land here: "done" must
        # get out whatever happens, or the app would wait for it forever.
        done = {"type": "done", "status": status, "elapsed_ms": int((time.monotonic() - st.started) * 1000),
                "truncated": self.truncated, "kernel_lost": False}
        for _attempt in range(3):
            try:
                _emit(done)
                break
            except KeyboardInterrupt:
                continue
        self.running = False
        _CURRENT_ID = None
        self.send_variables()

    # introspection -----------------------------------------------------------
    def send_variables(self):
        items = []
        total = 0
        budget = time.monotonic() + 0.5
        for name in sorted(self.ns.keys(), key=str.lower):
            if name.startswith("_") or name in self.HIDDEN:
                continue
            value = self.ns[name]
            if isinstance(value, (types.ModuleType, types.FunctionType, types.BuiltinFunctionType, type,
                                  types.MethodType)):
                continue
            total += 1
            if len(items) >= MAX_VARIABLES or time.monotonic() > budget:
                continue
            try:
                tname, shape, preview = _var_info(name, value)
            except Exception:  # noqa: BLE001
                tname, shape, preview = type(value).__name__, "", ""
            items.append({"name": name, "type": tname, "shape": shape, "preview": preview})
        _emit({"type": "variables", "items": items, "total": total})

    def resolve(self, dotted):
        parts = [p for p in str(dotted).strip().split(".") if p]
        if not parts or not all(p.isidentifier() for p in parts):
            raise LookupError(dotted)
        if parts[0] in self.ns:
            obj = self.ns[parts[0]]
        elif hasattr(builtins, parts[0]):
            obj = getattr(builtins, parts[0])
        else:
            raise LookupError(dotted)
        for p in parts[1:]:
            obj = getattr(obj, p)
        return obj

    def inspect_name(self, cmd):
        import inspect as _inspect  # noqa: PLC0415
        name = str(cmd.get("name", ""))
        reply = {"type": "inspect", "id": cmd.get("id", ""), "name": name, "found": False}
        try:
            obj = self.resolve(name)
        except Exception:  # noqa: BLE001
            _emit(reply)
            return
        reply["found"] = True
        reply["kind"] = type(obj).__name__
        try:
            reply["signature"] = name.split(".")[-1] + str(_inspect.signature(obj)) if callable(obj) else ""
        except (TypeError, ValueError):
            reply["signature"] = ""
        doc = ""
        if callable(obj) or isinstance(obj, types.ModuleType):
            try:
                doc = _inspect.getdoc(obj) or ""
            except Exception:  # noqa: BLE001
                doc = ""
        if len(doc) > MAX_DOC_CHARS:
            doc = doc[:MAX_DOC_CHARS] + "\n…"
        reply["doc"] = doc
        if not callable(obj) and not isinstance(obj, types.ModuleType):
            tname, shape, preview = _var_info(name, obj)
            reply["kind"], reply["shape"], reply["value"] = tname, shape, preview
        try:
            mod = _inspect.getmodule(obj)
            reply["module"] = mod.__name__ if mod else ""
        except Exception:  # noqa: BLE001
            reply["module"] = ""
        _emit(reply)

    def complete(self, cmd):
        import keyword  # noqa: PLC0415
        prefix = str(cmd.get("prefix", ""))
        head, _, tail = prefix.rpartition(".")
        names = []
        try:
            if head:
                names = dir(self.resolve(head))
            else:
                names = list(self.ns.keys()) + dir(builtins) + keyword.kwlist
        except Exception:  # noqa: BLE001
            names = []
        show_private = tail.startswith("_")
        matches = sorted({n for n in names if n.startswith(tail) and (show_private or not n.startswith("_"))},
                         key=str.lower)[:200]
        _emit({"type": "complete", "id": cmd.get("id", ""), "prefix": prefix, "matches": matches})

    def preview(self, cmd):
        name = str(cmd.get("name", ""))
        reply = {"type": "preview", "id": cmd.get("id", ""), "name": name, "found": False}
        try:
            obj = self.resolve(name)
        except Exception:  # noqa: BLE001
            _emit(reply)
            return
        reply["found"] = True
        table = _table_event(obj)
        if table is not None:
            table.pop("type", None)
            reply["table"] = table
        else:
            try:
                text = repr(obj)
            except Exception as exc:  # noqa: BLE001
                text = "<repr failed: %s>" % exc
            reply["text"] = text[:MAX_RESULT_CHARS]
        _emit(reply)

    # main loop ---------------------------------------------------------------
    def serve(self):
        signal.signal(signal.SIGINT, signal.default_int_handler)
        threading.Thread(target=self.reader, daemon=True).start()
        threading.Thread(target=self.flusher, daemon=True).start()
        _emit({"type": "ready", "python": sys.executable, "version": sys.version.split()[0], "pid": os.getpid(),
               "cwd": os.getcwd()})
        while True:
            try:
                cmd = self.commands.get()
            except KeyboardInterrupt:
                continue  # an interrupt that arrived after its cell ended
            op = cmd.get("op")
            try:
                if op == "shutdown":
                    return 0
                if op == "execute":
                    self.execute(cmd)
                elif op == "variables":
                    self.send_variables()
                elif op == "inspect":
                    self.inspect_name(cmd)
                elif op == "complete":
                    self.complete(cmd)
                elif op == "preview":
                    self.preview(cmd)
                elif op == "style":
                    if isinstance(cmd.get("style"), dict):
                        self.mpl_style = cmd["style"]
                        if self.mpl_styled:
                            self.apply_mpl_style(force=True)
            except KeyboardInterrupt:
                continue


def _private_command_stream():
    """The command pipe on a private descriptor, with descriptor 0 / the process's
    standard input pointed at the null device.

    The reader thread always has a read pending on the command pipe. On Windows a
    synchronous read pending on a pipe blocks every other call on that pipe from
    other threads - GetFileType included, which DLL start-up code runs on the
    standard handles: `import numpy` hung until the next command arrived. Reading
    from a duplicate and giving the process NUL as its standard input avoids that
    (and keeps cell code and the processes it starts away from the commands)."""
    try:
        fd = os.dup(sys.stdin.fileno())
        nul = os.open(os.devnull, os.O_RDONLY)
        os.dup2(nul, 0)  # on Windows this also re-points STD_INPUT_HANDLE
        os.close(nul)
        return open(fd, "r", encoding="utf-8-sig", errors="replace", closefd=True)
    except (OSError, ValueError, AttributeError):
        try:
            sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
        return sys.stdin


def _serve_kernel(args):
    global _PROTOCOL_OUT, _KERNEL  # noqa: PLW0603
    real_out = sys.stdout
    try:
        real_out.reconfigure(encoding="utf-8", errors="replace", line_buffering=False)
    except (AttributeError, ValueError):
        pass
    commands = _private_command_stream()
    _PROTOCOL_OUT = real_out
    if args.cwd and os.path.isdir(args.cwd):
        os.chdir(args.cwd)
    sys.path.insert(0, os.getcwd())
    kernel = _Kernel(os.getcwd(), commands)
    _KERNEL = kernel
    kernel.streams = {"stdout": _CellStream("stdout", kernel, 1), "stderr": _CellStream("stderr", kernel, 2)}
    sys.stdout = kernel.streams["stdout"]
    sys.stderr = kernel.streams["stderr"]
    sys.stdin = _NoInput()
    _install_inline_backend(kernel)
    try:
        import warnings  # noqa: PLC0415
        warnings.filterwarnings("ignore", message=".*non-interactive.*cannot be shown")
        warnings.filterwarnings("ignore", message=".*FigureCanvasAgg is non-interactive.*")
    except Exception:  # noqa: BLE001
        pass
    return kernel.serve()


# ── Entry point ────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["run", "kernel"])
    parser.add_argument("--cancel-file", default="")
    parser.add_argument("--timeout", type=int, default=1800)
    parser.add_argument("--cwd", default="")
    parser.add_argument("--python", default="")
    parser.add_argument("--child", action="store_true")
    parser.add_argument("--json-stdin", action="store_true")
    args = parser.parse_args()

    if args.command == "kernel":
        return _serve_kernel(args)

    for name in ("stdout", "stderr"):
        stream = getattr(sys, name)
        try:
            stream.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
        except (AttributeError, ValueError):
            pass
        setattr(sys, name, _Tracked(stream))

    raw = sys.stdin.buffer.read()
    code = raw.decode("utf-8-sig", "replace")  # a writer may prefix a BOM
    prefix = []
    if args.json_stdin:
        try:
            payload = json.loads(code)
            prefix = [str(p) for p in payload.get("prefix", []) if str(p).strip()]
            code = str(payload.get("code", ""))
        except (ValueError, AttributeError) as exc:
            _emit({"type": "started", "python": sys.executable, "version": sys.version.split()[0]})
            _emit({"type": "error", "ename": "KernelError", "evalue": "Bad cell payload: %s" % exc,
                   "traceback": [], "line": 0})
            _emit({"type": "done", "status": "error", "elapsed_ms": 0})
            return 0

    if args.python and not args.child:
        same = os.path.normcase(os.path.realpath(args.python)) == os.path.normcase(os.path.realpath(sys.executable))
        if not same:
            _emit({"type": "started", "python": args.python, "version": ""})
            return _run_child(args, raw)

    if args.cwd and os.path.isdir(args.cwd):
        os.chdir(args.cwd)
        sys.path.insert(0, args.cwd)
    # No GUI windows from a cell: plt.show() must not block a headless process.
    os.environ.setdefault("MPLBACKEND", "Agg")

    if not args.child:
        _emit({"type": "started", "python": sys.executable, "version": sys.version.split()[0]})
    return _run_in_process(args, code, prefix)


if __name__ == "__main__":
    sys.exit(main())
