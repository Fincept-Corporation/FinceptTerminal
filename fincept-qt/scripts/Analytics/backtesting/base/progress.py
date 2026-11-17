"""
Progress lines, cooperative cancellation and child-process lifetime for backtesting providers.

PROGRESS PROTOCOL (contract v2, "PROGRESS LINES")
    One line per update on STDERR, at most 4 per second per phase:

        @@BT_PROGRESS {"phase":"optimize","done":240,"total":500,"message":"Optimising 240 / 500",
                       "best":1.21,"etaSec":14.2,"workers":4}

    phase   data | run | optimize | walk_forward | monte_carlo | sweep | metrics
    done    int, units of work finished in this phase (symbols, combinations, runs, paths)
    total   int | null (null = indeterminate)
    message short plain text for the status line
    best    number | null (best objective so far, optimise / walk-forward)
    etaSec  number | null (seconds left, from a monotonic clock; null until it can be estimated)
    workers int | null (worker processes in use)

    WHY STDERR. BacktestingService (C++) reads the provider's STDOUT through
    python::extract_json, which keeps the LAST complete JSON value. Progress objects on
    stdout would be harmless while a run finishes normally, but when a process dies before
    its final envelope (crash, kill), the last progress object would be parsed as the
    result - {"phase": ...} has no "success" key and BacktestingService treats a missing
    "success" as true, so the screen would render an empty "successful" result. STDERR is
    only used by the host as error text (its tail) and is never parsed as JSON, so progress
    there cannot break today's single-JSON contract. The host passes no stream callback yet
    (BacktestingService.cpp: run_with_options without on_line); when it does it reads STDERR
    lines starting with PREFIX and ignores everything else. Unknown keys must be ignored by
    readers; unparseable lines are ignored.

CANCELLATION
    The host passes `cancelFile` with every run and creates that file on CANCEL.
    CancelToken(request).check() exits the process with code 3 after printing the standard
    cancelled envelope ({"success": false, "error": "Cancelled by user", "errorCode":
    "CANCELLED", "cancelled": true}). Registered cleanups (process pools) run first.

CHILD LIFETIME
    bind_child_lifetime() puts this process into a Windows job object with
    KILL_ON_JOB_CLOSE (fincept_connector._bind_child_lifetime), so pool workers die with the
    provider even when it is killed with TerminateProcess. On Linux each pool worker asks the
    kernel for SIGKILL when its parent dies (worker_parent_death_signal()).
"""

from __future__ import annotations

import json
import math
import os
import sys
import threading
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

PREFIX = '@@BT_PROGRESS '
PHASES = ('data', 'run', 'optimize', 'walk_forward', 'monte_carlo', 'sweep', 'metrics')
MAX_RATE_HZ = 4.0

_lock = threading.Lock()
_last_emit: Dict[str, float] = {}
_enabled = os.environ.get('FINCEPT_BT_PROGRESS', '1') not in ('0', 'false', 'no')


def _finite(x) -> Optional[float]:
    try:
        v = float(x)
    except (TypeError, ValueError):
        return None
    return v if math.isfinite(v) else None


def format_line(phase: str, done: int, total: Optional[int] = None, message: str = '',
                best: Optional[float] = None, eta_sec: Optional[float] = None,
                workers: Optional[int] = None) -> str:
    """One progress line (without the trailing newline). Pure; used by emit() and tests."""
    obj = {
        'phase': str(phase),
        'done': int(done),
        'total': None if total is None else int(total),
        'message': str(message or '')[:160],
        'best': _finite(best),
        'etaSec': None if eta_sec is None else round(max(0.0, float(eta_sec)), 1),
        'workers': None if workers is None else int(workers),
    }
    return PREFIX + json.dumps(obj, separators=(',', ':'), ensure_ascii=False, allow_nan=False)


def parse_line(line: str) -> Optional[Dict[str, Any]]:
    """The progress object of a stderr line, or None (the reference reader for the host)."""
    s = (line or '').strip()
    if not s.startswith(PREFIX.strip()):
        return None
    try:
        obj = json.loads(s[len(PREFIX.strip()):].strip())
    except ValueError:
        return None
    if not isinstance(obj, dict) or 'phase' not in obj or 'done' not in obj:
        return None
    return obj


def _write(line: str) -> None:
    try:
        err = sys.stderr
        err.write(line + '\n')
        err.flush()
    except (OSError, ValueError, AttributeError):
        pass  # stderr closed (host went away): progress is best effort


def emit(phase: str, done: int, total: Optional[int] = None, message: str = '',
         best: Optional[float] = None, eta_sec: Optional[float] = None,
         workers: Optional[int] = None, force: bool = False) -> bool:
    """Emit a progress line unless the phase emitted one less than 1/MAX_RATE_HZ s ago.

    force=True always emits (first and last line of a phase). Returns True when written.
    """
    if not _enabled:
        return False
    now = time.monotonic()
    with _lock:
        last = _last_emit.get(phase, -1e9)
        if not force and now - last < 1.0 / MAX_RATE_HZ:
            return False
        _last_emit[phase] = now
    _write(format_line(phase, done, total, message, best, eta_sec, workers))
    return True


class Progress:
    """Rate-limited progress of one phase with an ETA from a monotonic clock.

        p = Progress('optimize', total=500, workers=4)
        for i, row in enumerate(results, 1):
            p.update(i, f'Optimising {i} / 500', best=best_so_far)
        p.finish('Optimisation done')
    """

    def __init__(self, phase: str, total: Optional[int] = None, *, workers: Optional[int] = None,
                 message: str = '', emit_start: bool = True):
        self.phase = phase
        self.total = None if total is None else int(total)
        self.workers = workers
        self.t0 = time.monotonic()
        self.done = 0
        self.best: Optional[float] = None
        if emit_start:
            emit(phase, 0, self.total, message or f'{phase} started', workers=workers, force=True)

    def eta(self) -> Optional[float]:
        if not self.total or self.done <= 0:
            return None
        elapsed = time.monotonic() - self.t0
        return elapsed / self.done * max(0, self.total - self.done)

    def update(self, done: int, message: str = '', best: Optional[float] = None, force: bool = False) -> bool:
        self.done = int(done)
        if best is not None:
            self.best = best
        return emit(self.phase, self.done, self.total, message, self.best, self.eta(), self.workers, force=force)

    def finish(self, message: str = '') -> None:
        done = self.total if self.total is not None else self.done
        emit(self.phase, done, self.total, message or f'{self.phase} done', self.best, 0.0, self.workers, force=True)


# ── Cancellation ────────────────────────────────────────────────────────────

_cleanups: List[Callable[[], None]] = []


def register_cleanup(fn: Callable[[], None]) -> None:
    """Run `fn` before a cancelled process exits (e.g. terminate a process pool)."""
    _cleanups.append(fn)


def unregister_cleanup(fn: Callable[[], None]) -> None:
    try:
        _cleanups.remove(fn)
    except ValueError:
        pass


def cancelled_envelope() -> Dict[str, Any]:
    return {'success': False, 'error': 'Cancelled by user', 'errorCode': 'CANCELLED', 'cancelled': True,
            'details': {'traceback': None, 'logs': [], 'symbol': None}}


def cancel_exit() -> None:
    """Run cleanups, print the cancelled envelope as the single stdout line, exit with code 3."""
    for fn in list(_cleanups):
        try:
            fn()
        except Exception:
            pass
    try:
        out = sys.__stdout__
        out.write(json.dumps(cancelled_envelope()) + '\n')
        out.flush()
    finally:
        os._exit(3)


class CancelToken:
    """Cheap, rate-limited check of the host's cancel flag file."""

    def __init__(self, request: Optional[Dict[str, Any]] = None, path: Optional[str] = None,
                 min_interval: float = 0.2):
        p = path
        if p is None and isinstance(request, dict):
            p = request.get('cancelFile')
        self.path = str(p) if p else None
        self.min_interval = float(min_interval)
        self._last = -1e9
        self._flag = False

    def cancelled(self) -> bool:
        if self._flag:
            return True
        if not self.path:
            return False
        now = time.monotonic()
        if now - self._last < self.min_interval:
            return False
        self._last = now
        self._flag = os.path.exists(self.path)
        return self._flag

    def check(self) -> None:
        """Exit with the cancelled envelope (code 3) when the host cancelled this run."""
        if self.cancelled():
            cancel_exit()


# ── Child-process lifetime ──────────────────────────────────────────────────

_job_handle = None


def _scripts_dir() -> Path:
    # scripts/Analytics/backtesting/base/progress.py -> scripts/
    return Path(__file__).resolve().parents[3]


def ensure_scripts_on_path() -> None:
    """scripts/ (fincept_json, fincept_connector) importable from any provider."""
    d = str(_scripts_dir())
    if d not in sys.path:
        sys.path.append(d)


def bind_child_lifetime() -> bool:
    """Tie child processes (pool workers) to this process. Idempotent. True when bound.

    Windows: a job object with KILL_ON_JOB_CLOSE (fincept_connector._bind_child_lifetime);
    children inherit it and die when this process ends for any reason. When the host
    already put the provider in a job (PythonRunner does), nesting works on Windows 8+ and
    the host's tree kill covers the rest. POSIX: no-op here; workers call
    worker_parent_death_signal() themselves.
    """
    global _job_handle
    if _job_handle is not None:
        return True
    if os.name != 'nt':
        return False
    try:
        ensure_scripts_on_path()
        from fincept_connector import _bind_child_lifetime  # noqa: PLC0415
        h = _bind_child_lifetime()
        if h:
            _job_handle = h  # keep the handle referenced for the life of the process
            return True
    except Exception:
        pass
    return False


def worker_parent_death_signal() -> None:
    """In a pool worker on Linux: SIGKILL this worker when its parent dies (prctl)."""
    if not sys.platform.startswith('linux'):
        return
    try:
        import ctypes
        import signal
        libc = ctypes.CDLL('libc.so.6', use_errno=True)
        libc.prctl(1, int(signal.SIGKILL), 0, 0, 0)  # PR_SET_PDEATHSIG
    except Exception:
        pass
