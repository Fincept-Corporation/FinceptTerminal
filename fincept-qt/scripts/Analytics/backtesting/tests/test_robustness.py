"""base.robustness: grids and sampling, walk-forward layout and driver, DSR / PBO calibration on
noise and planted edges, the process pool (spawn), progress lines and cancellation."""
import json
import math
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

from base import robustness as R
from base.progress import parse_line
from helpers import periodic_evaluate, sleepy_evaluate, sma_context, sma_evaluate

HERE = Path(__file__).resolve().parent


# ── grids ───────────────────────────────────────────────────────────────────

def test_param_values_count_formula():
    assert R.param_values({'min': 1.0, 'max': 1.3, 'step': 0.1}) == [1.0, 1.1, 1.2, 1.3]   # never overshoots
    assert R.param_values({'min': 1.5, 'max': 2.5, 'step': 0.5}) == [1.5, 2.0, 2.5]
    v = R.param_values({'min': 10, 'max': 35, 'step': 5})
    assert v == [10, 15, 20, 25, 30, 35] and all(isinstance(x, int) for x in v)
    assert R.param_values({'min': 0.02, 'max': 0.1, 'step': 0.02}) == [0.02, 0.04, 0.06, 0.08, 0.1]
    assert R.param_values({'min': 5, 'max': 1, 'step': 2}) == [1, 3, 5]


def test_sampling_is_seeded_without_replacement():
    ranges = {'fastPeriod': {'min': 5, 'max': 54, 'step': 1}, 'slowPeriod': {'min': 60, 'max': 150, 'step': 10}}
    all_, info = R.sample_combinations(ranges, 'grid', 500)
    assert len(all_) == 500 and not info['truncated'] and info['totalCombinations'] == 500
    a, ia = R.sample_combinations(ranges, 'grid', 100, seed=3)
    b, _ = R.sample_combinations(ranges, 'grid', 100, seed=3)
    c, _ = R.sample_combinations(ranges, 'random', 100, seed=4)
    assert a == b and a != c and ia['truncated']
    assert len({json.dumps(x, sort_keys=True) for x in a}) == 100
    assert len({x['fastPeriod'] for x in a}) > 30               # spread over the grid, not the first rows
    with pytest.raises(ValueError):
        R.sample_combinations({}, 'grid', 10)
    with pytest.raises(ValueError):
        R.normalize_objective('banana')
    assert R.normalize_objective('Sharpe') == 'sharpe' and R.normalize_objective('return') == 'return'


# ── walk-forward layout (A0.5) ──────────────────────────────────────────────

def test_walk_forward_windows_tile_history():
    n = 2516
    ws = R.walk_forward_windows(n, 5, 0.7, purge_bars=5, warmup_bars=60)
    tested = sum(w.test_end - w.test_start for w in ws)
    assert tested / n >= 0.6
    assert ws[-1].test_end == n
    for a, b in zip(ws, ws[1:]):
        assert b.test_start == a.test_end                        # contiguous, no overlap
    for w in ws:
        assert w.train_end + 5 == w.test_start                   # purge gap
        assert w.train_end - w.train_start == ws[0].train_end - ws[0].train_start   # rolling: fixed length
        assert w.test_data_start == w.test_start - 60            # warm-up loaded before the test window
    an = R.walk_forward_windows(n, 5, 0.7, mode='anchored')
    assert all(w.train_start == 0 for w in an)
    emb = R.walk_forward_windows(n, 5, 0.7, embargo_bars=10)
    assert emb[0].excluded == [] and emb[2].excluded


def test_walk_forward_too_short_is_a_plain_error():
    with pytest.raises(ValueError, match='warm-up'):
        R.walk_forward_windows(400, 5, 0.7, warmup_bars=200)
    with pytest.raises(ValueError, match='too few'):
        R.walk_forward_windows(60, 5, 0.7)


def test_identical_behaviour_gives_degradation_one():
    n = 2000
    ws = R.walk_forward_windows(n, 5, 0.7)
    combos = [{'k': 1.0}, {'k': 0.5}]
    out = R.run_walk_forward(periodic_evaluate, combos, ws, context={'n': n}, objective='sharpe', ppy=252,
                             dates=pd.bdate_range('2010-01-01', periods=n), workers=1, progress=False)
    assert out['degradationRatio'] == pytest.approx(1.0, abs=0.05)
    assert out['walkForwardEfficiency'] == pytest.approx(1.0, abs=0.05)
    assert out['schemaVersion'] == 2 and out['nWindows'] == 5 and out['oosCoverage'] >= 0.6


def test_walk_forward_on_real_strategy(spy):
    ctx = sma_context()
    n = len(ctx['close'])
    combos, _ = R.sample_combinations({'fastPeriod': {'min': 10, 'max': 30, 'step': 10},
                                       'slowPeriod': {'min': 50, 'max': 100, 'step': 50}})
    ws = R.walk_forward_windows(n, 4, 0.7, warmup_bars=100)
    out = R.run_walk_forward(sma_evaluate, combos, ws, context=ctx, objective='sharpe', ppy=252,
                             dates=ctx['dates'], workers=1, progress=False)
    assert len(out['windows']) == 4 and all(w['bestParams'] for w in out['windows'])
    assert all(w['inSampleSharpe'] is not None for w in out['windows'])
    assert len(out['oosEquity']) == sum(w['testBars'] for w in out['windows'])
    comp = np.prod([1 + w['outOfSampleReturn'] for w in out['windows']]) - 1
    assert out['compoundedOosReturn'] == pytest.approx(comp, rel=1e-9)
    assert out['oosEquity'][-1]['equity'] == pytest.approx(100000 * (1 + comp), rel=1e-6)


def test_purged_kfold_covers_every_bar_once():
    folds = R.purged_kfold(1003, 5, purge_bars=5, embargo_bars=10)
    tests = [f['test'] for f in folds]
    assert tests[0][0] == 0 and tests[-1][1] == 1003
    assert all(a[1] == b[0] for a, b in zip(tests, tests[1:]))
    f = folds[2]
    a, b = f['test']
    for s, e in f['train']:
        assert e <= a - 5 or s >= b + 15


# ── overfitting statistics (A0.4) ───────────────────────────────────────────

def test_dsr_does_not_flag_noise():
    rng = np.random.default_rng(11)
    flagged = 0
    reps = 30
    for _ in range(reps):
        X = rng.normal(0, 0.01, size=(2520, 500))
        srs = X.mean(0) / X.std(0, ddof=1)
        best = int(np.argmax(srs))
        d = R.deflated_sharpe(X[:, best], srs, 500)
        flagged += d['deflatedSharpe'] > 0.95
    assert flagged / reps <= 0.05


def test_pbo_of_noise_is_about_half():
    rng = np.random.default_rng(5)
    vals = [R.pbo_cscv(rng.normal(0, 0.01, size=(1600, 100)))['pbo'] for _ in range(5)]
    assert abs(np.mean(vals) - 0.5) <= 0.1


def test_planted_edge_is_flagged():
    """500 parameter variants of a strategy with a real edge (drift 10 %/yr, vol 15 % -> true Sharpe
    0.67). A 0.67 Sharpe is itself only ~2 standard errors from 0 over 10 years, so no test can flag
    every sample: measured detection 70 % (10 y) / 85 % (20 y); the undeflated PSR reaches 80 % / 90 %.
    Asserted: over 20-year samples DSR flags the edge most of the time and deflating for 500 correlated
    trials costs little power (noise is flagged <= 5 %: test_dsr_does_not_flag_noise)."""
    T, N, reps = 5040, 500, 20
    dsr_hits = psr_hits = 0
    for seed in range(reps):
        rng = np.random.default_rng(100 + seed)
        mkt = rng.normal(0.10 / 252, 0.15 / math.sqrt(252), T)
        w = rng.uniform(0.5, 1.5, N)
        X = mkt[:, None] * w[None, :] + rng.normal(0, 0.02 / math.sqrt(252), (T, N))
        srs = X.mean(0) / X.std(0, ddof=1)
        d = R.deflated_sharpe(X[:, int(np.argmax(srs))], srs, N)
        dsr_hits += d['deflatedSharpe'] > 0.95
        psr_hits += d['probabilisticSharpe'] > 0.95
    assert dsr_hits / reps >= 0.75
    assert dsr_hits >= psr_hits - 3
    # a single strong edge among noise trials is selected in-sample AND holds out of sample
    rng = np.random.default_rng(2)
    Y = rng.normal(0, 0.01, size=(2000, 100))
    Y[:, 7] += 2.0 * 0.01 / math.sqrt(252)
    assert R.pbo_cscv(Y)['pbo'] < 0.1


def test_pbo_memory_is_bounded_and_chunking_does_not_change_it():
    """5,000 trials used to allocate ~10 arrays of 2,048 splits x 5,000 trials (1.3 GB peak in the
    vectorbt 5,000-combination optimise); the pass size now shrinks with the trial count."""
    assert R.pbo_chunk(500) == 2048 and R.pbo_chunk(5000) == 300 and R.pbo_chunk(10 ** 7) == 16
    assert R.pbo_chunk(5000) * 5000 <= R.PBO_CELLS
    rng = np.random.default_rng(9)
    X = rng.normal(0, 0.01, size=(800, 60))
    a, b = R.pbo_cscv(X, chunk=2048), R.pbo_cscv(X, chunk=17)
    assert a['pbo'] == b['pbo'] and a['logits'] == b['logits'] and a['probOosLoss'] == b['probOosLoss']


def test_expected_max_sharpe_matches_simulation():
    rng = np.random.default_rng(1)
    T, N = 1000, 200
    maxima = [(lambda X: (X.mean(0) / X.std(0, ddof=1)).max())(rng.normal(size=(T, N))) for _ in range(40)]
    assert R.expected_max_sharpe(N, 1.0 / T) == pytest.approx(np.mean(maxima), rel=0.05)


# ── optimisation driver ─────────────────────────────────────────────────────

RANGES = {'fastPeriod': {'min': 10, 'max': 30, 'step': 10}, 'slowPeriod': {'min': 50, 'max': 150, 'step': 50}}


def test_run_optimization_in_process(capsys):
    ctx = sma_context()
    combos, info = R.sample_combinations(RANGES, 'grid', 100)
    out = R.run_optimization(sma_evaluate, combos, context=ctx, objective='sharpe', ppy=252, dates=ctx['dates'],
                             info=info, workers=1, holdout_fraction=0.3)
    objs = [r['objectiveValue'] for r in out['allResults']]
    assert objs == sorted(objs, reverse=True) and len(objs) == 9
    assert out['bestParameters'] == out['allResults'][0]['parameters']
    ov = out['overfitting']
    assert ov['nTrials'] == 9 and 0 <= ov['deflatedSharpe'] <= 1 and ov['pbo'] is None   # < 10 trials
    assert out['statisticalSignificance']['method'] == 'deflated_sharpe'
    assert set(out['holdout']) >= {'splitDate', 'inSample', 'outOfSample', 'bestParameters'}
    assert out['stability']['neighbours'] >= 1
    err = capsys.readouterr()
    lines = [parse_line(x) for x in err.err.splitlines() if x.startswith('@@BT_PROGRESS')]
    assert lines and lines[-1]['done'] == 9 and lines[-1]['phase'] == 'optimize'
    assert '@@BT_PROGRESS' not in err.out
    again = R.run_optimization(sma_evaluate, combos, context=ctx, objective='sharpe', ppy=252, dates=ctx['dates'],
                               info=info, workers=1, progress=False)
    assert [r['objectiveValue'] for r in again['allResults']] == objs


def test_process_pool_equals_in_process():
    ctx = sma_context(n_bars=800)
    combos, info = R.sample_combinations(RANGES, 'grid', 100)
    a = R.run_optimization(sma_evaluate, combos, context=ctx, objective='sortino', ppy=252, workers=1,
                           progress=False)
    b = R.run_optimization(sma_evaluate, combos, context=ctx, objective='sortino', ppy=252, workers=2,
                           progress=False)
    assert b['workers'] == 2
    assert [r['parameters'] for r in a['allResults']] == [r['parameters'] for r in b['allResults']]
    assert np.allclose([r['objectiveValue'] for r in a['allResults']],
                       [r['objectiveValue'] for r in b['allResults']], rtol=1e-6)


def test_progress_rate_limited_and_on_stderr(capsys):
    combos = [{'k': i} for i in range(400)]
    t0 = time.monotonic()
    R.run_optimization(sleepy_evaluate, combos, context={'sleep': 0.004}, objective='return', ppy=252, workers=1)
    dt = time.monotonic() - t0
    cap = capsys.readouterr()
    lines = [x for x in cap.err.splitlines() if x.startswith('@@BT_PROGRESS')]
    assert len(lines) >= 5
    assert len(lines) <= 4 * dt + 3          # <= 4 per second (+ forced first / last lines)
    assert not any(x.startswith('@@BT_PROGRESS') for x in cap.out.splitlines())


def test_failing_combinations_are_reported():
    def ev(params, context, window):
        if params['k'] == 2:
            raise RuntimeError('boom')
        return {'returns': np.full(30, 0.001 * params['k'])}
    out = R.run_optimization(ev, [{'k': 1}, {'k': 2}, {'k': 3}], objective='return', workers=1, progress=False)
    assert out['iterations'] == 3 and len(out['allResults']) == 2
    assert any(w['code'] == 'TRIALS_FAILED' for w in out['warnings'])


def test_cancel_terminates_pool_without_orphans(tmp_path):
    psutil = pytest.importorskip('psutil')
    flag = tmp_path / 'cancel.flag'
    script = tmp_path / 'opt_cancel.py'
    script.write_text(
        'import sys\n'
        f'sys.path.insert(0, {str(HERE.parent)!r}); sys.path.insert(0, {str(HERE)!r})\n'
        'from base import robustness as R\nfrom base.progress import CancelToken\nfrom helpers import sleepy_evaluate\n'
        'if __name__ == "__main__":\n'
        f'    tok = CancelToken({{"cancelFile": {str(flag)!r}}})\n'
        '    R.run_optimization(sleepy_evaluate, [{"k": i} for i in range(400)], context={"sleep": 0.05},\n'
        '                       objective="return", workers=2, cancel=tok)\n'
        '    print("finished")\n', encoding='utf-8')
    p = subprocess.Popen([sys.executable, str(script)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    time.sleep(6.0)
    kids = [c.pid for c in psutil.Process(p.pid).children(recursive=True)]
    flag.write_text('x')
    out, err = p.communicate(timeout=60)
    assert p.returncode == 3, err[-500:]
    lines = [x for x in out.splitlines() if x.strip()]
    assert len(lines) == 1 and json.loads(lines[0])['errorCode'] == 'CANCELLED'
    time.sleep(1.0)
    alive = [k for k in kids if psutil.pid_exists(k) and psutil.Process(k).status() != psutil.STATUS_ZOMBIE]
    assert not alive, f'orphaned workers: {alive}'
