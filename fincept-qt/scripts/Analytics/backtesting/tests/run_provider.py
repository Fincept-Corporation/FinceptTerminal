"""Run one backtesting provider exactly like the terminal does and check its output hygiene.

    python run_provider.py <engine> <command> <payload.json | inline JSON> [--timeout S] [--data-dir DIR]

BacktestingService (C++) runs `<venv python> Analytics/backtesting/<engine>/<engine>_provider.py
<command> --stdin` with the JSON payload on stdin and the scripts folder as working directory, then
parses the LAST complete JSON value of stdout. Checks (contract R1/R2): exactly one non-empty stdout
line, strict JSON (no NaN / Infinity), the envelope, progress lines only on stderr.

Library use:  from run_provider import run;  r = run('vectorbt', 'run_backtest', payload_dict)
              r.rc, r.envelope, r.data, r.problems, r.progress, r.wall_s, r.peak_rss_mb
"""
import json
import os
import re
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

HERE = Path(__file__).resolve().parent
SCRIPTS = HERE.parent.parent.parent          # scripts/


@dataclass
class RunResult:
    rc: int
    stdout: str
    stderr: str
    wall_s: float
    peak_rss_mb: float
    envelope: Optional[Dict[str, Any]] = None
    problems: List[str] = field(default_factory=list)
    progress: List[Dict[str, Any]] = field(default_factory=list)

    @property
    def data(self) -> Optional[Dict[str, Any]]:
        e = self.envelope or {}
        return e.get('data') if isinstance(e.get('data'), dict) else None

    @property
    def ok(self) -> bool:
        return bool(self.envelope and self.envelope.get('success') is True and not self.problems)


def run(engine: str, command: str, payload: Any, *, python: Optional[str] = None, timeout: float = 900,
        env: Optional[Dict[str, str]] = None, data_dir: Optional[str] = None) -> RunResult:
    script = SCRIPTS / 'Analytics' / 'backtesting' / engine / f'{engine}_provider.py'
    if not script.exists():
        raise FileNotFoundError(script)
    text = payload if isinstance(payload, str) else json.dumps(payload)
    e = dict(os.environ)
    e.update({'PYTHONIOENCODING': 'utf-8', 'PYTHONUNBUFFERED': '1', 'PYTHONDONTWRITEBYTECODE': '1'})
    for k in ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS'):
        e.setdefault(k, '2')
    if data_dir:
        e['FINCEPT_DATA_DIR'] = data_dir
    if env:
        e.update(env)
    t0 = time.time()
    p = subprocess.Popen([python or sys.executable, str(script), command, '--stdin'], stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=e, cwd=str(SCRIPTS))
    peak = [0.0]

    def watch():
        try:
            import psutil
            pr = psutil.Process(p.pid)
            while p.poll() is None:
                try:
                    peak[0] = max(peak[0], sum(x.memory_info().rss for x in [pr] + pr.children(recursive=True)))
                except Exception:
                    pass
                time.sleep(0.1)
        except Exception:
            pass
    threading.Thread(target=watch, daemon=True).start()
    try:
        out, err = p.communicate(text.encode('utf-8'), timeout=timeout)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        p.kill()
        out, err = p.communicate()
        rc = -9
    res = RunResult(rc, out.decode('utf-8', 'replace'), err.decode('utf-8', 'replace'), time.time() - t0,
                    peak[0] / 1e6)
    lines = [ln for ln in res.stdout.replace('\r', '').split('\n') if ln.strip()]
    if len(lines) != 1:
        res.problems.append(f'stdout has {len(lines)} non-empty lines (must be exactly 1)')
    if re.search(r'(?<![A-Za-z"])(NaN|-?Infinity)(?![A-Za-z"])', res.stdout):
        res.problems.append('NaN / Infinity token on stdout')
    if any(ln.startswith('@@BT_PROGRESS') for ln in lines):
        res.problems.append('progress line on stdout (belongs on stderr)')

    def bad_const(c):
        raise ValueError(f'non-standard JSON constant {c}')
    try:
        res.envelope = json.loads(lines[-1] if lines else '', parse_constant=bad_const)
    except Exception as ex:
        res.problems.append(f'stdout is not strict JSON: {ex}')
    if isinstance(res.envelope, dict):
        if 'success' not in res.envelope:
            res.problems.append('envelope has no "success"')
        elif res.envelope['success'] is False and not res.envelope.get('error'):
            res.problems.append('failure envelope without an error message')
        d = res.envelope.get('data')
        if res.envelope.get('success') is True and isinstance(d, dict) and d.get('success') is False:
            res.problems.append('{"success": true, "data": {"success": false}} (R2)')
    if rc not in (0, 3):
        res.problems.append(f'exit code {rc}')
    sys.path.insert(0, str(HERE.parent))
    from base.progress import parse_line
    res.progress = [x for x in (parse_line(ln) for ln in res.stderr.splitlines()) if x]
    return res


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    engine, command, payload = argv[1:4]
    opts = argv[4:]
    timeout = float(opts[opts.index('--timeout') + 1]) if '--timeout' in opts else 900
    data_dir = opts[opts.index('--data-dir') + 1] if '--data-dir' in opts else None
    if os.path.exists(payload):
        payload = Path(payload).read_text(encoding='utf-8')
    r = run(engine, command, payload, timeout=timeout, data_dir=data_dir)
    d = r.data or {}
    print(f'{engine}/{command}: rc={r.rc} ok={r.ok} wall={r.wall_s:.1f}s rss={r.peak_rss_mb:.0f}MB '
          f'bytes={len(r.stdout)} progress={len(r.progress)} problems={r.problems or "none"} keys={len(d)}')
    if r.envelope and r.envelope.get('success') is False:
        print('error:', r.envelope.get('error'), '| code:', r.envelope.get('errorCode'))
    return 0 if r.ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
