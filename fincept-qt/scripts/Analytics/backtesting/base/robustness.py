"""
Engine-agnostic robustness tools (decision 6): optimisation driver, walk-forward driver,
overfitting statistics (Deflated Sharpe Ratio, PBO by CSCV), hold-out split, parameter
stability. Monte Carlo lives in base.monte_carlo.

THE ENGINE SUPPLIES ONE CALLBACK
    def evaluate(params: dict, context, window: Optional[dict]) -> dict
        params   one parameter combination ({'fastPeriod': 20, 'slowPeriod': 50})
        context  whatever the engine needs (bars, request...), passed ONCE per worker
        window   None = the full period; else {'dataStart': i0, 'scoreStart': i1, 'end': i2}
                 (bar indexes): run on bars [i0, i2) - the bars before i1 are warm-up - and
                 return results for bars [i1, i2) only
        returns  {'returns': per-bar returns of the scored bars (1-D, len = scored bars; the
                  first may be 0), optional 'trades': int, 'winRate', 'profitFactor', 'sqn'}
    For worker processes `evaluate` must be a module-level function of an importable module
    (not the provider's __main__) and `context` must be picklable; otherwise use workers=1.

OPTIMISE
    combos, info = sample_combinations(param_ranges, method='grid'|'random', max_iterations=500, seed=7)
    out = run_optimization(evaluate, combos, context=ctx, objective='sharpe', ppy=252,
                           dates=index, request=req, info=info)      # -> contract `optimize` data
  Grid values use the screen's count formula n = floor((max - min) / step + 1e-9) + 1 (float-
  safe, never overshoots max); integer ranges stay integers. Random search and grids larger
  than maxIterations are seeded samples WITHOUT replacement (reproducible; the seed is echoed).
  A process pool of min(4, cores) workers (FINCEPT_BT_MAX_WORKERS / request maxWorkers lower
  it), spawn-safe on Windows, BLAS threads = 1 per worker, deterministic (results ordered by
  combination; numpy / random seeded per trial), progress lines, cancel within ~0.2 s,
  early stop on a time budget or (random search) on no improvement for `patience` trials.

  Overfitting (contract `overfitting`): nTrials, sharpeStd, expectedMaxSharpe (the best
  annualised Sharpe expected from nTrials zero-skill trials), deflatedSharpe (probability that
  the best trial's true Sharpe > 0 after accounting for the number of trials, the variance of
  trial Sharpes, and the best trial's skewness / kurtosis / length - Bailey & Lopez de Prado
  2014), pbo (probability of backtest overfitting by combinatorially symmetric cross-
  validation, Bailey, Borwein, Lopez de Prado & Zhu 2015, 16 blocks; null below 10 trials or
  200 bars), effectiveTrials. The old "statisticalSignificance" z-test of the best result
  against the set it was picked from (true for noise 89 % of the time at 30 combinations) is
  replaced by a deflated-Sharpe based block with the same keys.

WALK-FORWARD
    windows = walk_forward_windows(n_bars, n_windows=5, train_ratio=0.7, mode='rolling',
                                   purge_bars=0, embargo_bars=0, warmup_bars=50)
    out = run_walk_forward(evaluate, combos, windows, context=ctx, objective='sharpe', ...)
  Test windows tile the history after the first training window (no gaps, no overlap); each
  fold optimises on its training window and runs the best set on its test window with warm-up
  bars loaded before both. Rolling = fixed-length training windows; anchored = expanding from
  the first bar. purge_bars: bars between a training window's end and its test window's start
  that belong to neither (so indicators / labels on training bars cannot reach into the test).
  embargo_bars: bars right after each test window that are left out of every LATER training
  window (Lopez de Prado's embargo, for serially correlated returns). Reports per-fold IS and
  OOS figures, the stitched OOS equity, degradationRatio = mean OOS Sharpe / mean IS Sharpe and
  walkForwardEfficiency = annualised compounded OOS return / annualised mean IS return.
"""

from __future__ import annotations

import itertools
import math
import os
import random
import sys
import time
from dataclasses import dataclass, field
from statistics import NormalDist
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple

import numpy as np

from base import metrics as M

EULER_GAMMA = 0.5772156649015329
OBJECTIVES = ('sharpe', 'sortino', 'calmar', 'return', 'cagr', 'max_drawdown', 'omega', 'profit_factor',
              'win_rate', 'sqn')
_OBJ_ALIASES = {'sharpe_ratio': 'sharpe', 'sharperatio': 'sharpe', 'sortino_ratio': 'sortino',
                'calmar_ratio': 'calmar', 'total_return': 'return', 'totalreturn': 'return', 'returns': 'return',
                'drawdown': 'max_drawdown', 'maxdrawdown': 'max_drawdown', 'min_drawdown': 'max_drawdown',
                'profitfactor': 'profit_factor', 'winrate': 'win_rate', 'omega_ratio': 'omega'}
_N = NormalDist()


def _log(msg: str) -> None:
    print(f'[robustness] {msg}', file=sys.stderr, flush=True)


# ════════════════════════════════════════════════════════════════════════════
# Parameter grids
# ════════════════════════════════════════════════════════════════════════════

def _decimals(x: float) -> int:
    s = repr(float(x))
    if 'e' in s or 'E' in s:
        return 10
    return len(s.split('.')[1].rstrip('0')) if '.' in s else 0


def param_values(spec: Any) -> List[Any]:
    """Values of one parameter range. spec: {'min', 'max', 'step'} | list of values | scalar.
    Count = floor((max - min) / step + 1e-9) + 1 (the screen's formula); value i = min + i*step
    rounded to the inputs' decimals; integers stay integers when min and step are integral."""
    if isinstance(spec, (list, tuple)):
        return list(spec)
    if not isinstance(spec, dict):
        return [spec]
    if 'values' in spec and isinstance(spec['values'], (list, tuple)):
        return list(spec['values'])
    lo = float(spec.get('min', spec.get('start', 0)))
    hi = float(spec.get('max', spec.get('end', lo)))
    st = float(spec.get('step', 1) or 1)
    if hi < lo:
        lo, hi = hi, lo
    if not st > 0:
        raise ValueError('a parameter range needs a positive step')
    n = int(math.floor((hi - lo) / st + 1e-9)) + 1
    if n > 1_000_000:
        raise ValueError(f'a parameter range with {n:,} values is too large')
    dec = max(_decimals(lo), _decimals(st))
    integral = float(lo).is_integer() and float(st).is_integer() and spec.get('type') != 'float'
    out = []
    for i in range(n):
        v = round(lo + i * st, dec)
        out.append(int(round(v)) if integral else float(v))
    return out


def param_grid(param_ranges: Dict[str, Any]) -> Tuple[List[str], List[List[Any]]]:
    if not isinstance(param_ranges, dict) or not param_ranges:
        raise ValueError('No parameter ranges supplied (expected paramRanges with min / max / step per parameter).')
    names = list(param_ranges.keys())
    return names, [param_values(param_ranges[k]) for k in names]


def grid_size(value_lists: Sequence[Sequence[Any]]) -> int:
    n = 1
    for v in value_lists:
        n *= max(1, len(v))
    return n


def _decode(index: int, names: List[str], values: List[List[Any]]) -> Dict[str, Any]:
    out = {}
    for name, vals in zip(reversed(names), reversed(values)):
        index, r = divmod(index, len(vals))
        out[name] = vals[r]
    return {k: out[k] for k in names}


def sample_combinations(param_ranges: Dict[str, Any], method: str = 'grid', max_iterations: Optional[int] = None,
                        seed: Optional[int] = None) -> Tuple[List[Dict[str, Any]], Dict[str, Any]]:
    """(combinations, info). info: method, totalCombinations, iterations, truncated, seed, paramGrid."""
    names, values = param_grid(param_ranges)
    total = grid_size(values)
    meth = str(method or 'grid').strip().lower()
    if meth not in ('grid', 'random'):
        raise ValueError(f'optimisation method must be grid or random, not {method!r}')
    cap = int(float(max_iterations)) if max_iterations not in (None, '', 0) else total
    cap = max(1, cap)
    seed = int(seed) if seed is not None else 7
    rng = np.random.default_rng(seed)
    if meth == 'grid' and total <= cap:
        idx = list(range(total))
        truncated = False
    else:
        k = min(cap, total)
        if total <= 5_000_000:
            idx = sorted(rng.choice(total, size=k, replace=False).tolist())
        else:
            chosen = set()
            while len(chosen) < k:
                chosen.update(int(x) for x in rng.integers(0, total, size=k - len(chosen)))
            idx = sorted(chosen)
        truncated = k < total
    combos = [_decode(i, names, values) for i in idx]
    return combos, {'method': meth, 'totalCombinations': total, 'iterations': len(combos), 'truncated': truncated,
                    'seed': seed, 'paramGrid': {n: v for n, v in zip(names, values)},
                    'sampling': ('full grid' if not truncated and meth == 'grid' else
                                 f'seeded sample without replacement ({len(combos)} of {total})')}


# ════════════════════════════════════════════════════════════════════════════
# Objectives
# ════════════════════════════════════════════════════════════════════════════

def normalize_objective(name: Optional[str]) -> str:
    k = str(name or 'sharpe').strip().lower().replace(' ', '_')
    k = _OBJ_ALIASES.get(k, k)
    if k not in OBJECTIVES:
        raise ValueError(f'unknown optimisation objective {name!r}; choose one of {", ".join(OBJECTIVES)}')
    return k


def performance_of(returns: np.ndarray, ppy: float, rf_bar=0.0, extras: Optional[dict] = None,
                   years: Optional[float] = None) -> Dict[str, Optional[float]]:
    """The contract's per-row performance keys from a return series (+ trade extras)."""
    q = M.quick_stats(returns, ppy, rf_bar, years=years)
    ex = extras or {}
    return {'totalReturn': q['totalReturn'], 'cagr': q['cagr'], 'sharpeRatio': q['sharpeRatio'],
            'sortinoRatio': q['sortinoRatio'], 'calmarRatio': q['calmarRatio'], 'maxDrawdown': q['maxDrawdown'],
            'volatility': q['volatility'], 'totalTrades': ex.get('trades'), 'winRate': ex.get('winRate'),
            'profitFactor': ex.get('profitFactor'), 'omegaRatio': M.omega_ratio(returns, rf_bar)}


def objective_value(perf: Dict[str, Optional[float]], objective: str, extras: Optional[dict] = None) -> Optional[float]:
    o = objective
    ex = extras or {}
    if o == 'sharpe':
        return perf.get('sharpeRatio')
    if o == 'sortino':
        return perf.get('sortinoRatio')
    if o == 'calmar':
        return perf.get('calmarRatio')
    if o == 'return':
        return perf.get('totalReturn')
    if o == 'cagr':
        return perf.get('cagr')
    if o == 'max_drawdown':
        v = perf.get('maxDrawdown')
        return None if v is None else -v
    if o == 'omega':
        return perf.get('omegaRatio')
    if o == 'profit_factor':
        return ex.get('profitFactor')
    if o == 'win_rate':
        return ex.get('winRate')
    if o == 'sqn':
        return ex.get('sqn')
    return None


# ════════════════════════════════════════════════════════════════════════════
# Worker pool
# ════════════════════════════════════════════════════════════════════════════

_W: Dict[str, Any] = {}


def max_workers(request: Optional[dict] = None, n_tasks: Optional[int] = None) -> int:
    cores = os.cpu_count() or 1
    w = min(4, cores)
    env = os.environ.get('FINCEPT_BT_MAX_WORKERS')
    if env:
        try:
            w = max(1, min(w, int(env)))
        except ValueError:
            pass
    if isinstance(request, dict) and request.get('maxWorkers'):
        try:
            w = max(1, min(w, int(request['maxWorkers'])))
        except (TypeError, ValueError):
            pass
    if n_tasks is not None:
        w = max(1, min(w, n_tasks))
    return w


def _worker_init(evaluate, context, base_seed):
    from base.progress import worker_parent_death_signal
    worker_parent_death_signal()
    _W['evaluate'] = evaluate
    _W['context'] = context
    _W['seed'] = base_seed


def _run_one(evaluate, context, base_seed, task):
    idx, params, window = task
    s = (int(base_seed) * 1_000_003 + int(idx)) % (2 ** 32)
    np.random.seed(s)
    random.seed(s)
    t0 = time.perf_counter()
    try:
        out = evaluate(params, context, window)
        r = np.asarray(out.get('returns'), dtype=np.float64) if isinstance(out, dict) else np.asarray(out)
        extras = {k: out.get(k) for k in ('trades', 'winRate', 'profitFactor', 'sqn')} if isinstance(out, dict) else {}
        return idx, r.astype(np.float32), extras, None, time.perf_counter() - t0
    except Exception as e:  # one failing combination does not stop the search
        return idx, None, {}, f'{type(e).__name__}: {e}', time.perf_counter() - t0


def _run_chunk(tasks):
    return [_run_one(_W['evaluate'], _W['context'], _W['seed'], t) for t in tasks]


class TaskRunner:
    """Runs evaluate() over tasks in-process or in a capped spawn process pool."""

    def __init__(self, evaluate, context, *, workers: int = 1, seed: int = 7, cancel=None):
        self.evaluate = evaluate
        self.context = context
        self.workers = max(1, int(workers))
        self.seed = int(seed)
        self.cancel = cancel
        self.pool = None

    def _start_pool(self):
        import concurrent.futures as cf
        import multiprocessing as mp
        from base import progress as P
        P.bind_child_lifetime()
        saved = {}
        for k in ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS', 'NUMEXPR_NUM_THREADS',
                  'NUMBA_NUM_THREADS', 'FINCEPT_BT_PROGRESS'):
            saved[k] = os.environ.get(k)
            os.environ[k] = '0' if k == 'FINCEPT_BT_PROGRESS' else '1'
        try:
            self.pool = cf.ProcessPoolExecutor(max_workers=self.workers, mp_context=mp.get_context('spawn'),
                                               initializer=_worker_init,
                                               initargs=(self.evaluate, self.context, self.seed))
        finally:
            for k, v in saved.items():
                if v is None:
                    os.environ.pop(k, None)
                else:
                    os.environ[k] = v
        P.register_cleanup(self.shutdown)

    def shutdown(self):
        if self.pool is None:
            return
        pool, self.pool = self.pool, None
        try:
            procs = list(getattr(pool, '_processes', {}).values())
            pool.shutdown(wait=False, cancel_futures=True)
            for p in procs:
                try:
                    p.terminate()
                except Exception:
                    pass
        except Exception:
            pass
        from base import progress as P
        P.unregister_cleanup(self.shutdown)

    def run(self, tasks: List[tuple], on_result: Callable, time_budget_s: Optional[float] = None,
            stop: Optional[Callable[[], bool]] = None) -> bool:
        """Call on_result(idx, returns, extras, error) per task. Returns False when stopped early."""
        t_end = time.monotonic() + time_budget_s if time_budget_s else None
        if self.workers <= 1 or len(tasks) < 2:
            for t in tasks:
                if self.cancel is not None:
                    self.cancel.check()
                if (t_end and time.monotonic() > t_end) or (stop and stop()):
                    return False
                idx, r, ex, err, _ = _run_one(self.evaluate, self.context, self.seed, t)
                on_result(idx, r, ex, err)
            return True
        import concurrent.futures as cf
        self._start_pool()
        chunk = max(1, min(64, len(tasks) // (self.workers * 8) or 1))
        chunks = [tasks[i:i + chunk] for i in range(0, len(tasks), chunk)]
        pending = set()
        it = iter(chunks)
        complete = True
        try:
            for _ in range(self.workers * 2):
                c = next(it, None)
                if c is None:
                    break
                pending.add(self.pool.submit(_run_chunk, c))
            while pending:
                done, pending = cf.wait(pending, timeout=0.2, return_when=cf.FIRST_COMPLETED)
                if self.cancel is not None:
                    self.cancel.check()
                for f in done:
                    for idx, r, ex, err, _ in f.result():
                        on_result(idx, r, ex, err)
                stopping = (t_end and time.monotonic() > t_end) or (stop and stop())
                if stopping:
                    complete = False
                    for f in pending:
                        f.cancel()
                    pending = {f for f in pending if not f.cancelled()}
                    continue
                while len(pending) < self.workers * 2:
                    c = next(it, None)
                    if c is None:
                        break
                    pending.add(self.pool.submit(_run_chunk, c))
        finally:
            self.shutdown()
        return complete


# ════════════════════════════════════════════════════════════════════════════
# Overfitting statistics
# ════════════════════════════════════════════════════════════════════════════

def expected_max_sharpe(n_trials: int, sharpe_var: float) -> float:
    """E[max of n_trials zero-skill Sharpe estimates] (same units as sqrt(sharpe_var)),
    Bailey & Lopez de Prado (2014), eq. for SR_0."""
    n = int(n_trials)
    if n <= 1 or not sharpe_var > 0:
        return 0.0
    a = _N.inv_cdf(1.0 - 1.0 / n)
    b = _N.inv_cdf(1.0 - 1.0 / (n * math.e))
    return math.sqrt(sharpe_var) * ((1.0 - EULER_GAMMA) * a + EULER_GAMMA * b)


def probabilistic_sharpe(sr: float, sr_star: float, n_obs: int, skew: float, kurt_raw: float) -> Optional[float]:
    """P(true SR > sr_star) for a per-bar Sharpe estimate sr over n_obs bars (PSR, Bailey & Lopez de
    Prado 2012). kurt_raw is the raw (non-excess) kurtosis."""
    if n_obs < 3 or sr is None:
        return None
    den = 1.0 - skew * sr + (kurt_raw - 1.0) / 4.0 * sr * sr
    if not den > 0:
        return None
    z = (sr - sr_star) * math.sqrt(n_obs - 1) / math.sqrt(den)
    return float(_N.cdf(z))


def deflated_sharpe(best_returns: np.ndarray, trial_sharpes_per_bar: np.ndarray,
                    n_trials: Optional[int] = None) -> Dict[str, Optional[float]]:
    """Deflated Sharpe Ratio of the selected (best) trial. trial_sharpes_per_bar: NON-annualised
    Sharpe of every trial (the variance of these sets the selection hurdle)."""
    r = np.asarray(best_returns, dtype=float)
    r = r[np.isfinite(r)]
    srs = np.asarray(trial_sharpes_per_bar, dtype=float)
    srs = srs[np.isfinite(srs)]
    n = int(n_trials or len(srs))
    out = {'deflatedSharpe': None, 'expectedMaxSharpePerBar': None, 'bestSharpePerBar': None,
           'probabilisticSharpe': None, 'zScore': None}
    if len(r) < 3 or len(srs) < 1:
        return out
    sd = float(np.std(r, ddof=1))
    if not sd > 0:
        return out
    sr = float(np.mean(r) / sd)
    var = float(np.var(srs, ddof=1)) if len(srs) > 1 else 0.0
    sr0 = expected_max_sharpe(n, var)
    skew = M.skewness(r) or 0.0
    kurt = (M.excess_kurtosis(r) or 0.0) + 3.0
    den = 1.0 - skew * sr + (kurt - 1.0) / 4.0 * sr * sr
    out.update({'bestSharpePerBar': sr, 'expectedMaxSharpePerBar': sr0,
                'probabilisticSharpe': probabilistic_sharpe(sr, 0.0, len(r), skew, kurt)})
    if den > 0:
        z = (sr - sr0) * math.sqrt(len(r) - 1) / math.sqrt(den)
        out['zScore'] = float(z)
        out['deflatedSharpe'] = float(_N.cdf(z))
    return out


def effective_trials(R: np.ndarray, max_cols: int = 400) -> Optional[float]:
    """N_eff = rho + (1 - rho) * N from the mean pairwise correlation rho of trial returns
    (a simple stand-in for clustering; N_eff = N for independent trials, 1 for identical)."""
    X = np.asarray(R, dtype=float)
    n = X.shape[1]
    if n < 2:
        return float(n)
    if n > max_cols:
        X = X[:, np.linspace(0, n - 1, max_cols).astype(int)]
    sd = X.std(axis=0)
    X = X[:, sd > 0]
    if X.shape[1] < 2:
        return 1.0
    C = np.corrcoef(X, rowvar=False)
    m = C.shape[0]
    rho = float((C.sum() - m) / (m * (m - 1)))
    rho = min(1.0, max(0.0, rho))
    return float(rho + (1.0 - rho) * n)


PBO_CELLS = 1_500_000   # splits x trials per working array (~12 MB): bounds pbo_cscv's memory


def pbo_chunk(n_trials: int, chunk: int = 2048) -> int:
    """Splits evaluated per pass of pbo_cscv: `chunk`, reduced so a (splits x trials) array stays near
    PBO_CELLS elements. (~10 such arrays live at once: 2048 splits x 5,000 trials took 1.3 GB.)"""
    return int(max(16, min(int(chunk), PBO_CELLS // max(1, int(n_trials)))))


def pbo_cscv(R: np.ndarray, n_blocks: int = 16, min_bars: int = 200, min_trials: int = 10,
             chunk: int = 2048) -> Dict[str, Any]:
    """Probability of Backtest Overfitting by combinatorially symmetric cross-validation.

    R: (T bars x N trials) per-bar returns. The T rows are cut into S contiguous blocks (S even,
    16 by default, fewer when blocks would be shorter than 20 bars); for every one of the C(S, S/2)
    ways to pick half the blocks as in-sample, the trial with the best IS Sharpe is located in the
    out-of-sample ranking: w = rank / (N + 1), logit = ln(w / (1 - w)). PBO = share of splits with
    logit <= 0 (the IS winner is at or below the OOS median)."""
    X = np.asarray(R, dtype=float)
    T, N = X.shape
    out: Dict[str, Any] = {'pbo': None, 'blocks': None, 'splits': None, 'logits': None,
                           'probOosLoss': None, 'reason': None}
    if N < min_trials:
        out['reason'] = f'fewer than {min_trials} trials'
        return out
    if T < min_bars:
        out['reason'] = f'fewer than {min_bars} bars'
        return out
    S = int(n_blocks)
    while S > 4 and T // S < 20:
        S -= 2
    S -= S % 2
    X = np.where(np.isfinite(X), X, 0.0)
    edges = np.linspace(0, T, S + 1).astype(int)
    bs = np.add.reduceat(X, edges[:-1], axis=0)               # (S, N) sums
    bq = np.add.reduceat(X * X, edges[:-1], axis=0)           # (S, N) sums of squares
    bn = np.diff(edges).astype(float)                         # (S,)
    combos = np.array(list(itertools.combinations(range(S), S // 2)), dtype=np.int64)
    C = len(combos)
    mask = np.zeros((C, S), dtype=float)
    mask[np.repeat(np.arange(C), S // 2), combos.ravel()] = 1.0
    logits = np.empty(C)
    oos_best = np.empty(C)
    step = pbo_chunk(N, chunk)
    for a in range(0, C, step):
        mk = mask[a:a + step]
        res = []
        for msk in (mk, 1.0 - mk):
            s = msk @ bs
            q = msk @ bq
            n = (msk @ bn)[:, None]
            mu = s / n
            var = np.maximum(q / n - mu * mu, 0.0) * n / np.maximum(n - 1.0, 1.0)
            sd = np.sqrt(var)
            res.append(np.where(sd > 0, mu / np.where(sd > 0, sd, 1.0), -np.inf))
        is_sr, oos_sr = res
        best = np.argmax(is_sr, axis=1)
        ob = oos_sr[np.arange(len(best)), best]
        rank = (oos_sr < ob[:, None]).sum(axis=1) + 0.5 * ((oos_sr == ob[:, None]).sum(axis=1) - 1) + 1.0
        w = rank / (N + 1.0)
        logits[a:a + len(best)] = np.log(w / (1.0 - w))
        oos_best[a:a + len(best)] = ob
    out.update({'pbo': float(np.mean(logits <= 0.0)), 'blocks': S, 'splits': int(C),
                'logits': {q: float(np.percentile(logits, p)) for q, p in
                           (('p5', 5), ('p25', 25), ('p50', 50), ('p75', 75), ('p95', 95))},
                'probOosLoss': float(np.mean(oos_best < 0.0))})
    return out


def overfitting_stats(R: np.ndarray, best_idx: int, ppy: float, n_trials: Optional[int] = None,
                      rf_bar=0.0) -> Dict[str, Any]:
    """Contract `overfitting` block from the (T x N) return matrix of every trial."""
    X = np.asarray(R, dtype=float)
    T, N = X.shape
    srs = M.sharpe_columns(X, ppy, rf_bar, annualize=False)
    n = int(n_trials or N)
    ds = deflated_sharpe(X[:, best_idx] - (rf_bar if np.ndim(rf_bar) == 0 else np.asarray(rf_bar)), srs, n)
    pb = pbo_cscv(X)
    ok = np.isfinite(srs)
    sharpe_std = float(np.std(srs[ok], ddof=1) * math.sqrt(ppy)) if ok.sum() > 1 else None
    emax = ds['expectedMaxSharpePerBar']
    return {
        'nTrials': n,
        'sharpeStd': sharpe_std,
        'expectedMaxSharpe': (float(emax * math.sqrt(ppy)) if emax is not None else None),
        'bestSharpe': (float(ds['bestSharpePerBar'] * math.sqrt(ppy)) if ds['bestSharpePerBar'] is not None else None),
        'deflatedSharpe': ds['deflatedSharpe'],
        'deflatedSharpeZ': ds['zScore'],
        'probabilisticSharpe': ds['probabilisticSharpe'],
        'pbo': pb['pbo'],
        'pboBlocks': pb['blocks'],
        'pboSplits': pb['splits'],
        'pboLogits': pb['logits'],
        'pboReason': pb['reason'],
        'probOosLoss': pb['probOosLoss'],
        'effectiveTrials': effective_trials(X) if N > 1 else float(N),
        'bars': int(T),
        'method': 'Deflated Sharpe Ratio (Bailey & Lopez de Prado 2014); PBO by CSCV (Bailey et al. 2015)',
    }


def significance_block(overfit: Dict[str, Any], objectives: Sequence[float]) -> Optional[Dict[str, Any]]:
    """The legacy `statisticalSignificance` keys the screen shows today, filled from the deflated
    Sharpe (selection-aware) instead of the naive z-test of the best against its own grid."""
    ds = overfit.get('deflatedSharpe')
    if ds is None:
        return None
    objs = np.asarray([o for o in objectives if o is not None and math.isfinite(o)], dtype=float)
    return {'method': 'deflated_sharpe', 'significant': bool(ds > 0.95), 'pValue': float(1.0 - ds),
            'zScore': overfit.get('deflatedSharpeZ'), 'nTested': int(overfit.get('nTrials') or len(objs)),
            'bestVsMean': float(objs.max() - objs.mean()) if len(objs) else None,
            'note': 'p-value = 1 - Deflated Sharpe Ratio: the chance a zero-skill strategy picked as the best of '
                    'this many trials would look this good.'}


def plateau_score(rows: List[Dict[str, Any]], best_params: Dict[str, Any],
                  grid: Dict[str, List[Any]]) -> Dict[str, Any]:
    """Mean objective of the best set's grid neighbours (every parameter within one step) divided
    by the best objective. ~1 = a broad plateau, << 1 = an isolated peak."""
    names = list(grid.keys())
    pos = {n: {repr(v): i for i, v in enumerate(grid[n])} for n in names}
    bi = {n: pos[n].get(repr(best_params.get(n))) for n in names}
    if any(v is None for v in bi.values()):
        return {'plateauScore': None, 'neighbours': 0}
    best_obj = None
    vals = []
    for r in rows:
        p = r['parameters']
        ix = {n: pos[n].get(repr(p.get(n))) for n in names}
        if any(v is None for v in ix.values()):
            continue
        d = [abs(ix[n] - bi[n]) for n in names]
        if max(d) == 0:
            best_obj = r['objectiveValue']
        elif max(d) <= 1:
            vals.append(r['objectiveValue'])
    if not vals or best_obj is None or best_obj == 0:
        return {'plateauScore': None, 'neighbours': len(vals)}
    return {'plateauScore': float(np.mean(vals) / best_obj), 'neighbours': len(vals)}


# ════════════════════════════════════════════════════════════════════════════
# Optimisation driver
# ════════════════════════════════════════════════════════════════════════════

def _sensitivity(rows, names):
    objs = np.array([r['objectiveValue'] for r in rows], dtype=float)
    sens, spread = {}, {}
    for k in names:
        vals = np.array([M.finite_or_none(r['parameters'].get(k)) if not isinstance(r['parameters'].get(k), bool)
                         else float(r['parameters'].get(k)) for r in rows], dtype=float)
        ok = np.isfinite(vals)
        if ok.sum() > 2 and np.std(vals[ok]) > 1e-12 and np.std(objs[ok]) > 1e-12:
            c = np.corrcoef(vals[ok], objs[ok])[0, 1]
            sens[k] = float(c) if np.isfinite(c) else 0.0
        else:
            sens[k] = 0.0
        groups: Dict[str, List[float]] = {}
        for r in rows:
            groups.setdefault(repr(r['parameters'].get(k)), []).append(r['objectiveValue'])
        means = [sum(v) / len(v) for v in groups.values() if v]
        spread[k] = float(max(means) - min(means)) if len(means) > 1 else 0.0
    return sens, spread


def run_optimization(evaluate: Callable, combos: List[Dict[str, Any]], *, context: Any = None,
                     objective: str = 'sharpe', ppy: float = 252.0, dates=None, rf: Any = 0.0,
                     info: Optional[Dict[str, Any]] = None, request: Optional[dict] = None,
                     workers: Optional[int] = None, seed: int = 7, holdout_fraction: float = 0.0,
                     time_budget_s: Optional[float] = None, patience: Optional[int] = None,
                     cancel=None, progress: bool = True, max_rows: int = 2000) -> Dict[str, Any]:
    """Evaluate every combination on the full period, rank by `objective`, and attach the
    overfitting statistics. Returns the contract `optimize` data (minus engine-specific blocks)."""
    from base.progress import Progress
    obj = normalize_objective(objective)
    n = len(combos)
    if n == 0:
        raise ValueError('no parameter combinations to evaluate')
    w = workers if workers is not None else max_workers(request, n)
    rows: List[Optional[Dict[str, Any]]] = [None] * n
    rets: List[Optional[np.ndarray]] = [None] * n
    errors: List[str] = []
    best = {'v': -math.inf, 'since': 0}
    prog = Progress('optimize', total=n, workers=w, message=f'Optimising {n} combinations') if progress else None
    years = None
    idx_dates = M.to_datetime_index(dates) if dates is not None else None
    if idx_dates is not None and len(idx_dates) > 1:
        years = (idx_dates[-1] - idx_dates[0]).days / M.CALENDAR_DAYS
    rf_bar_cache: Dict[int, Any] = {}

    def rf_for(length):
        if length not in rf_bar_cache:
            rf_bar_cache[length] = M.rf_per_bar(length, rf, ppy) if np.isscalar(rf) else \
                M.rf_per_bar(length, np.asarray(rf)[-length:], ppy)
        return rf_bar_cache[length]
    done = {'n': 0}

    def on_result(i, r, extras, err):
        done['n'] += 1
        if err is not None or r is None:
            errors.append(f'{combos[i]}: {err}')
        else:
            rr = np.asarray(r, dtype=float)
            perf = performance_of(rr, ppy, rf_for(len(rr)), extras, years=years)
            v = objective_value(perf, obj, extras)
            rows[i] = {'parameters': combos[i], 'objectiveValue': v, 'performance': perf}
            rets[i] = r
            if v is not None and v > best['v']:
                best['v'] = v
                best['since'] = 0
            else:
                best['since'] += 1
        if prog:
            prog.update(done['n'], f'Optimising {done["n"]} / {n}', best=best['v'] if best['v'] > -math.inf else None)

    stop = (lambda: best['since'] >= patience) if patience else None
    runner = TaskRunner(evaluate, context, workers=w, seed=seed, cancel=cancel)
    t0 = time.perf_counter()
    complete = runner.run([(i, c, None) for i, c in enumerate(combos)], on_result, time_budget_s, stop)
    elapsed = time.perf_counter() - t0
    if prog:
        prog.finish(f'{done["n"]} combinations evaluated')
    valid = [i for i in range(n) if rows[i] is not None and rows[i]['objectiveValue'] is not None]
    warnings = []
    if errors:
        warnings.append({'code': 'TRIALS_FAILED', 'severity': 'warn', 'symbol': None,
                         'message': f'{len(errors)} of {n} combinations failed (first: {errors[0][:160]}).'})
    if not complete:
        warnings.append({'code': 'SEARCH_STOPPED_EARLY', 'severity': 'info', 'symbol': None,
                         'message': f'The search stopped after {done["n"]} of {n} combinations '
                                    f'({"time budget" if time_budget_s else "no improvement"}).'})
    if not valid:
        raise ValueError('every parameter combination failed: ' + (errors[0] if errors else 'no objective value'))
    valid.sort(key=lambda i: rows[i]['objectiveValue'], reverse=True)
    best_i = valid[0]
    T = max(len(rets[i]) for i in valid)
    R = np.zeros((T, len(valid)), dtype=np.float32)
    for c, i in enumerate(valid):
        x = rets[i]
        R[T - len(x):, c] = x
    overfit = overfitting_stats(R, 0, ppy, n_trials=done['n'], rf_bar=rf_for(T))
    ordered = [rows[i] for i in valid]
    names = list(combos[0].keys())
    varying = [k for k in names if len({repr(c.get(k)) for c in combos}) > 1]
    sens, spread = _sensitivity(ordered, names)
    grid = (info or {}).get('paramGrid') or {k: sorted({c[k] for c in combos}, key=lambda x: (str(type(x)), x))
                                             for k in names}
    out = {
        'schemaVersion': 2,
        'objective': obj,
        'method': (info or {}).get('method', 'grid'),
        'iterations': done['n'],
        'totalCombinations': (info or {}).get('totalCombinations', n),
        'truncated': bool((info or {}).get('truncated')) or not complete,
        'seed': (info or {}).get('seed', seed),
        'sampling': (info or {}).get('sampling'),
        'bestParameters': dict(rows[best_i]['parameters']),
        'bestObjectiveValue': rows[best_i]['objectiveValue'],
        'bestPerformance': dict(rows[best_i]['performance']),
        'allResults': ordered[:max_rows],
        'resultsReturned': min(len(ordered), max_rows),
        'paramNames': varying or names,
        'paramGrid': grid,
        'parameterSensitivity': sens,
        'parameterSpread': spread,
        'stability': plateau_score(ordered, rows[best_i]['parameters'], grid),
        'overfitting': overfit,
        'workers': w,
        'elapsedSec': round(elapsed, 3),
        'warnings': warnings,
    }
    sig = significance_block(overfit, [r['objectiveValue'] for r in ordered])
    if sig:
        out['statisticalSignificance'] = sig
    if holdout_fraction and 0 < holdout_fraction < 0.9:
        out['holdout'] = holdout_split(rows, rets, valid, obj, ppy, holdout_fraction, idx_dates, rf_for)
    return out


def holdout_split(rows, rets, valid, objective, ppy, fraction, dates, rf_for) -> Dict[str, Any]:
    """Pick the best combination on the first (1 - fraction) of the bars only, then report it on the
    untouched last `fraction` (the strategy runs continuously; only the scoring is split)."""
    T = max(len(rets[i]) for i in valid)
    cut = int(round(T * (1.0 - fraction)))
    best_v, best_i = -math.inf, valid[0]
    for i in valid:
        r = np.asarray(rets[i], dtype=float)
        r = r[-T:] if len(r) >= T else np.concatenate([np.zeros(T - len(r)), r])
        v = objective_value(performance_of(r[:cut], ppy, rf_for(cut)), objective)
        if v is not None and v > best_v:
            best_v, best_i = v, i
    r = np.asarray(rets[best_i], dtype=float)
    split_date = None
    if dates is not None and len(dates) == T:
        split_date = M._fmt_date(dates[cut])
    return {'splitDate': split_date, 'fraction': fraction, 'bestParameters': dict(rows[best_i]['parameters']),
            'inSample': performance_of(r[:cut], ppy, rf_for(cut)),
            'outOfSample': performance_of(r[cut:], ppy, rf_for(T - cut))}


# ════════════════════════════════════════════════════════════════════════════
# Walk-forward
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class WFWindow:
    index: int
    train_start: int
    train_end: int            # exclusive
    test_start: int
    test_end: int             # exclusive
    train_data_start: int     # train_start - warm-up (>= 0)
    test_data_start: int      # test_start - warm-up (>= 0)
    train_score_start: int    # first scored training bar (>= train_start; later for the first fold)
    excluded: List[Tuple[int, int]] = field(default_factory=list)   # embargoed bar ranges inside training

    def as_dict(self, dates=None) -> Dict[str, Any]:
        d = {'window': self.index + 1, 'trainBars': self.train_end - self.train_score_start,
             'testBars': self.test_end - self.test_start, 'trainStartIdx': self.train_start,
             'trainEndIdx': self.train_end - 1, 'testStartIdx': self.test_start, 'testEndIdx': self.test_end - 1}
        if dates is not None:
            f = M._fmt_date
            d.update({'trainStart': f(dates[self.train_score_start]), 'trainEnd': f(dates[self.train_end - 1]),
                      'testStart': f(dates[self.test_start]), 'testEnd': f(dates[self.test_end - 1])})
        return d


def walk_forward_windows(n_bars: int, n_windows: int = 5, train_ratio: float = 0.7, mode: str = 'rolling',
                         train_bars: Optional[int] = None, test_bars: Optional[int] = None,
                         purge_bars: int = 0, embargo_bars: int = 0, warmup_bars: int = 0,
                         min_scored_bars: int = 20) -> List[WFWindow]:
    """Fold layout; raises ValueError with a plain sentence when the history is too short."""
    mode = str(mode or 'rolling').lower()
    if mode not in ('rolling', 'anchored'):
        raise ValueError(f'walk-forward mode must be rolling or anchored, not {mode!r}')
    k = int(n_windows)
    if k < 1:
        raise ValueError('walk-forward needs at least one window')
    purge, emb, warm = max(0, int(purge_bars)), max(0, int(embargo_bars)), max(0, int(warmup_bars))
    if test_bars is None:
        r = float(train_ratio)
        if not 0.05 <= r <= 0.95:
            raise ValueError('the walk-forward train ratio must be between 0.05 and 0.95')
        test_bars = int((n_bars - purge) // (k + r / (1.0 - r)))
        if train_bars is None:
            train_bars = n_bars - purge - k * test_bars
    elif train_bars is None:
        train_bars = n_bars - purge - k * int(test_bars)
    train_bars, test_bars = int(train_bars), int(test_bars)
    if test_bars < min_scored_bars or train_bars < 1:
        raise ValueError(f'{n_bars} bars are too few for {k} walk-forward windows: each test window would have '
                         f'{test_bars} bars (at least {min_scored_bars} needed). Use fewer windows or a longer period.')
    first_test = n_bars - k * test_bars
    if first_test - purge < train_bars:
        raise ValueError(f'{n_bars} bars are too few for {k} windows of {test_bars} test bars after a '
                         f'{train_bars}-bar training window.')
    out = []
    for i in range(k):
        ts = first_test + i * test_bars
        te = ts + test_bars
        tr_end = ts - purge
        tr_start = 0 if mode == 'anchored' else max(0, tr_end - train_bars)
        score_start = max(tr_start, warm) if tr_start - warm < 0 else tr_start
        if tr_end - score_start < min_scored_bars:
            raise ValueError(f'Each training window has {tr_end - tr_start} bars but the strategy needs {warm} bars of '
                             f'warm-up, leaving {max(0, tr_end - score_start)} to score. Use fewer windows, a longer '
                             f'period, or shorter indicator periods.')
        excl = []
        if emb > 0:
            for j in range(i):
                a = first_test + (j + 1) * test_bars
                b = min(a + emb, tr_end)
                if a < b and a >= score_start:
                    excl.append((a, b))
        out.append(WFWindow(i, tr_start, tr_end, ts, te, max(0, tr_start - warm), max(0, ts - warm),
                            score_start, excl))
    return out


def purged_kfold(n_bars: int, n_folds: int = 5, purge_bars: int = 0, embargo_bars: int = 0) -> List[Dict[str, Any]]:
    """Purged k-fold CV splits (fixes zipline Z20): every bar is in exactly one test fold (the
    remainder goes to the last folds); training = all other bars minus `purge_bars` on BOTH sides
    of the test fold and minus `embargo_bars` after it. Ranges are [start, end) bar indexes."""
    k = int(n_folds)
    if k < 2 or n_bars < 2 * k:
        raise ValueError(f'{n_bars} bars cannot be split into {k} folds')
    edges = np.linspace(0, n_bars, k + 1).astype(int)
    out = []
    for i in range(k):
        a, b = int(edges[i]), int(edges[i + 1])
        lo = max(0, a - purge_bars)
        hi = min(n_bars, b + purge_bars + embargo_bars)
        train = [(0, lo)] if lo > 0 else []
        if hi < n_bars:
            train.append((hi, n_bars))
        out.append({'fold': i + 1, 'test': (a, b), 'train': train,
                    'trainBars': sum(e - s for s, e in train), 'testBars': b - a})
    return out


def run_walk_forward(evaluate: Callable, combos: List[Dict[str, Any]], windows: List[WFWindow], *,
                     context: Any = None, objective: str = 'sharpe', ppy: float = 252.0, dates=None,
                     rf: Any = 0.0, initial_capital: float = 100_000.0, mode: str = 'rolling',
                     purge_bars: int = 0, embargo_bars: int = 0, request: Optional[dict] = None,
                     workers: Optional[int] = None, seed: int = 7, cancel=None,
                     progress: bool = True) -> Dict[str, Any]:
    """Optimise on every training window, run the winner on its test window. Returns the
    contract `walk_forward` data."""
    from base.progress import Progress
    obj = normalize_objective(objective)
    K, N = len(windows), len(combos)
    total = K * N + K
    w = workers if workers is not None else max_workers(request, K * N)
    prog = Progress('walk_forward', total=total, workers=w, message=f'{K} windows x {N} combinations') \
        if progress else None
    idx = M.to_datetime_index(dates) if dates is not None else None
    done = {'n': 0}
    is_scores = np.full((K, N), -np.inf)
    is_perf: Dict[Tuple[int, int], Dict] = {}
    errors: List[str] = []

    def rf_bar(n):
        return M.rf_per_bar(n, rf, ppy) if np.isscalar(rf) else 0.0

    def train_task(wi, ci):
        win = windows[wi]
        return (wi * N + ci, combos[ci], {'dataStart': win.train_data_start, 'scoreStart': win.train_score_start,
                                          'end': win.train_end})

    def on_train(tid, r, extras, err):
        wi, ci = divmod(tid, N)
        done['n'] += 1
        if err is not None or r is None:
            errors.append(err or 'no returns')
        else:
            rr = np.asarray(r, dtype=float)
            win = windows[wi]
            if win.excluded:
                keep = np.ones(len(rr), dtype=bool)
                for a, b in win.excluded:
                    keep[a - win.train_score_start:b - win.train_score_start] = False
                rr = rr[keep]
            perf = performance_of(rr, ppy, rf_bar(len(rr)), extras)
            v = objective_value(perf, obj, extras)
            if v is not None:
                is_scores[wi, ci] = v
            is_perf[(wi, ci)] = perf
        if prog:
            prog.update(done['n'], f'window {wi + 1} / {K}: training {ci + 1} / {N}')

    runner = TaskRunner(evaluate, context, workers=w, seed=seed, cancel=cancel)
    runner.run([train_task(wi, ci) for wi in range(K) for ci in range(N)], on_train)
    best_ci = [int(np.argmax(is_scores[wi])) if np.isfinite(is_scores[wi]).any() else None for wi in range(K)]
    if all(b is None for b in best_ci):
        raise ValueError('walk-forward: every training run failed' + (f' ({errors[0]})' if errors else ''))
    oos: Dict[int, Tuple[np.ndarray, dict]] = {}

    def on_test(wi, r, extras, err):
        done['n'] += 1
        if err is None and r is not None:
            oos[wi] = (np.asarray(r, dtype=float), extras)
        else:
            errors.append(err or 'no returns')
        if prog:
            prog.update(done['n'], f'window {wi + 1} / {K}: testing')

    tests = [(wi, combos[best_ci[wi]], {'dataStart': windows[wi].test_data_start,
                                         'scoreStart': windows[wi].test_start, 'end': windows[wi].test_end})
             for wi in range(K) if best_ci[wi] is not None]
    TaskRunner(evaluate, context, workers=min(w, len(tests)), seed=seed + 1, cancel=cancel).run(tests, on_test)
    if prog:
        prog.finish('walk-forward done')

    rows = []
    stitched = []
    stitched_dates = []
    is_sharpes, oos_sharpes, oos_returns, is_ann = [], [], [], []
    for wi, win in enumerate(windows):
        d = win.as_dict(idx)
        ci = best_ci[wi]
        d['bestParams'] = dict(combos[ci]) if ci is not None else None
        ip = is_perf.get((wi, ci)) if ci is not None else None
        d['inSampleReturn'] = ip.get('totalReturn') if ip else None
        d['inSampleSharpe'] = ip.get('sharpeRatio') if ip else None
        d['inSampleObjective'] = float(is_scores[wi, ci]) if ci is not None and np.isfinite(is_scores[wi, ci]) else None
        if wi in oos:
            r, ex = oos[wi]
            op = performance_of(r, ppy, rf_bar(len(r)), ex)
            d.update({'outOfSampleReturn': op['totalReturn'], 'outOfSampleSharpe': op['sharpeRatio'],
                      'outOfSampleMaxDrawdown': op['maxDrawdown'], 'outOfSampleTrades': ex.get('trades'),
                      'outOfSampleCagr': op['cagr']})
            stitched.append(r)
            if idx is not None:
                stitched_dates.extend(idx[win.test_start:win.test_end])
            if op['sharpeRatio'] is not None:
                oos_sharpes.append(op['sharpeRatio'])
            if op['totalReturn'] is not None:
                oos_returns.append(op['totalReturn'])
        else:
            d.update({'outOfSampleReturn': None, 'outOfSampleSharpe': None, 'outOfSampleMaxDrawdown': None,
                      'outOfSampleTrades': None})
        if ip and ip.get('sharpeRatio') is not None:
            is_sharpes.append(ip['sharpeRatio'])
        if ip and ip.get('totalReturn') is not None and d['trainBars'] > 0:
            is_ann.append((1.0 + ip['totalReturn']) ** (ppy / d['trainBars']) - 1.0 if ip['totalReturn'] > -1 else -1.0)
        rows.append(d)

    allr = np.concatenate(stitched) if stitched else np.zeros(0)
    eq = initial_capital * np.cumprod(1.0 + allr) if len(allr) else np.zeros(0)
    oos_equity = [{'date': M._fmt_date(t), 'equity': round(float(v), 4)} for t, v in zip(stitched_dates, eq)] \
        if idx is not None else [{'date': str(i), 'equity': float(v)} for i, v in enumerate(eq)]
    compounded = float(eq[-1] / initial_capital - 1.0) if len(eq) else None
    oos_ann = ((1.0 + compounded) ** (ppy / len(allr)) - 1.0) if (compounded is not None and len(allr) and compounded > -1) \
        else None
    mean_is_sh = float(np.mean(is_sharpes)) if is_sharpes else None
    mean_oos_sh = float(np.mean(oos_sharpes)) if oos_sharpes else None
    mean_is_ann = float(np.mean(is_ann)) if is_ann else None
    pstab = {}
    for k in (combos[0].keys() if combos else []):
        vals = [r['bestParams'].get(k) for r in rows if r.get('bestParams')]
        num = [float(v) for v in vals if isinstance(v, (int, float)) and not isinstance(v, bool)]
        pstab[k] = {'values': vals, 'std': float(np.std(num, ddof=1)) if len(num) > 1 else None}
    warnings = []
    if errors:
        warnings.append({'code': 'TRIALS_FAILED', 'severity': 'warn', 'symbol': None,
                         'message': f'{len(errors)} walk-forward runs failed (first: {errors[0][:160]}).'})
    short = [r['window'] for r in rows if (r.get('testBars') or 0) < 60]
    if short:
        warnings.append({'code': 'SHORT_TEST_WINDOWS', 'severity': 'info', 'symbol': None,
                         'message': f'Test windows have {rows[0]["testBars"]} bars; Sharpe ratios on fewer than 60 bars '
                                    f'are very noisy.'})
    n_bars = len(idx) if idx is not None else (windows[-1].test_end if windows else 0)
    return {
        'schemaVersion': 2,
        'mode': mode, 'nWindows': K, 'purgeBars': int(purge_bars), 'embargoBars': int(embargo_bars),
        'objective': obj, 'combinationsPerWindow': N, 'windows': rows, 'oosEquity': oos_equity,
        'oosCoverage': (float(len(allr) / n_bars) if n_bars else None),
        'avgOosReturn': float(np.mean(oos_returns)) if oos_returns else None,
        'avgOosSharpe': mean_oos_sh,
        'avgIsSharpe': mean_is_sh,
        'oosReturnStd': float(np.std(oos_returns, ddof=1)) if len(oos_returns) > 1 else None,
        'compoundedOosReturn': compounded,
        'oosSharpe': M.sharpe_ratio(allr, rf_bar(len(allr)), ppy) if len(allr) > 2 else None,
        'robustnessScore': float(np.mean([1.0 if x > 0 else 0.0 for x in oos_returns])) if oos_returns else None,
        'degradationRatio': (mean_oos_sh / mean_is_sh) if (mean_oos_sh is not None and mean_is_sh and mean_is_sh > 0)
        else None,
        'walkForwardEfficiency': (oos_ann / mean_is_ann) if (oos_ann is not None and mean_is_ann and mean_is_ann > 0)
        else None,
        'paramStability': pstab, 'workers': w, 'warnings': warnings,
    }
