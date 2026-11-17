"""
Monte Carlo resampling of a backtest (command `monte_carlo`), shared by every engine.

    result = run_monte_carlo(run_backtest, request)          # legacy entry point every provider calls
    data   = monte_carlo_from_result(backtest_data, request)  # pure: from a normalised run_backtest result

METHODS (request mcMethod; v1 names accepted: 'bootstrap' = trade_bootstrap, 'reshuffle' = trade_shuffle;
an unknown name is an error, not a silent default)
  trade_bootstrap    draw the run's trades WITH replacement. Each trade's ACCOUNT return is
                     pnl / equity before its entry - the invested fraction comes from the result
                     (notional / equity), not from the request's sizing settings (B4).
  trade_shuffle      permute the same trades. The compounded final equity is identical in every
                     path (multiplication commutes), so final-equity percentiles are NOT reported
                     (null); only drawdown and ruin statistics vary.
  returns_bootstrap  draw per-bar returns of the equity curve with replacement (date axis).
  block_bootstrap    stationary bootstrap (Politis & Romano 1994) of per-bar returns with mean
                     block length mcBlockSize (default 20 bars): keeps volatility clustering;
                     works for buy-and-hold, multi-asset and allocation strategies. Default when
                     the request names no method.
  Trades still open at the end are included at their marked-to-market P&L (decision 4); with
  them, trade paths reconcile with the backtest's final equity when positions do not overlap.
  Seeded (mcSeed, default 42, echoed). probRuin = share of paths that ever fall below
  mcRuinLevel x initial capital (default 0.5).

OUTPUT: contract v2 keys + every v1 key the screen reads today (x, bands[{lower, upper,
lowerValues, upperValues}], median, originalPath, samplePaths, finalEquity, maxDrawdown,
tradeReturns, positionFraction, stats{...}).
"""

from __future__ import annotations

import math
from typing import Any, Callable, Dict, List, Optional

import numpy as np

METHODS = ('trade_bootstrap', 'trade_shuffle', 'returns_bootstrap', 'block_bootstrap')
_ALIASES = {'bootstrap': 'trade_bootstrap', 'reshuffle': 'trade_shuffle', 'shuffle': 'trade_shuffle',
            'trades': 'trade_bootstrap', 'returns': 'returns_bootstrap', 'block': 'block_bootstrap',
            'stationary': 'block_bootstrap', 'stationary_bootstrap': 'block_bootstrap'}
MAX_SIMS = 20000
_X_POINTS = 400
_CHUNK = 500


def normalize_method(name: Optional[str]) -> str:
    m = str(name or 'block_bootstrap').strip().lower().replace('-', '_').replace(' ', '_')
    m = _ALIASES.get(m, m)
    if m not in METHODS:
        raise ValueError(f'Unknown Monte Carlo method {name!r}; choose one of {", ".join(METHODS)} '
                         f'(or bootstrap / reshuffle).')
    return m


def _f(v) -> Optional[float]:
    try:
        x = float(v)
    except (TypeError, ValueError):
        return None
    return x if math.isfinite(x) else None


# ════════════════════════════════════════════════════════════════════════════
# Inputs from a backtest result
# ════════════════════════════════════════════════════════════════════════════

def trade_account_returns(trades: List[dict], equity: Optional[np.ndarray], dates: Optional[List[str]],
                          initial: float) -> Dict[str, Any]:
    """Per-trade return ON THE ACCOUNT: pnl / equity at the close before the entry bar (open
    positions at their marked P&L). Returns {returns, fractions (entry notional / equity),
    pnlPercent, nOpen}."""
    pos = {d: i for i, d in enumerate(dates or [])}
    out_r, fr, pct = [], [], []
    n_open = 0
    for t in sorted(trades, key=lambda x: str(x.get('entryDate') or '')):
        pnl = _f(t.get('pnl'))
        if pnl is None:
            continue
        i = t.get('entryBar')
        if i is None:
            i = pos.get(str(t.get('entryDate') or ''))
        if equity is not None and i is not None and len(equity):
            i = int(i)
            base = float(equity[i - 1]) if i > 0 else float(initial)
        else:
            base = float(initial)
        if not base > 0:
            continue
        out_r.append(pnl / base)
        q, px = _f(t.get('quantity')), _f(t.get('entryPrice'))
        if q is not None and px is not None:
            fr.append(abs(q * px) / base)
        p = _f(t.get('pnlPercent'))
        if p is not None:
            pct.append(p)
        if not t.get('exitDate'):
            n_open += 1
    return {'returns': np.asarray(out_r, dtype=float), 'fractions': np.asarray(fr, dtype=float),
            'pnlPercent': pct, 'nOpen': n_open}


# ════════════════════════════════════════════════════════════════════════════
# Path simulation
# ════════════════════════════════════════════════════════════════════════════

def stationary_bootstrap_indices(rng, n_paths: int, horizon: int, n: int, block: float) -> np.ndarray:
    """(n_paths, horizon) indices into a length-n series: blocks start uniformly at random, each step
    starts a new block with probability 1/block, else continues at the next index (circularly)."""
    p = 1.0 / max(1.0, float(block))
    new = rng.random((n_paths, horizon)) < p
    new[:, 0] = True
    starts = rng.integers(0, n, size=(n_paths, horizon))
    t = np.arange(horizon)
    bpos = np.maximum.accumulate(np.where(new, t[None, :], 0), axis=1)
    sv = np.take_along_axis(starts, bpos, axis=1)
    return (sv + (t[None, :] - bpos)) % n


def simulate_paths(method: str, values: np.ndarray, *, n_sims: int, seed: int, initial: float,
                   block_size: float = 20.0, horizon: Optional[int] = None, ruin_level: float = 0.5,
                   x_points: int = _X_POINTS) -> Dict[str, Any]:
    """Resample `values` (per-trade account returns or per-bar returns) into n_sims paths of
    `horizon` steps. Returns finals, max drawdowns, ruin flags, the paths sampled on an x grid."""
    v = np.asarray(values, dtype=float)
    v = v[np.isfinite(v)]
    n = len(v)
    H = int(horizon or n)
    rng = np.random.default_rng(int(seed))
    xs = np.unique(np.linspace(0, H, min(H + 1, x_points)).round().astype(int))
    finals = np.empty(n_sims)
    mdds = np.empty(n_sims)
    mins = np.empty(n_sims)
    grid = np.empty((n_sims, len(xs)))
    for a in range(0, n_sims, _CHUNK):
        m = min(_CHUNK, n_sims - a)
        if method == 'trade_shuffle':
            idx = np.argsort(rng.random((m, n)), axis=1)[:, :H]
        elif method == 'block_bootstrap':
            idx = stationary_bootstrap_indices(rng, m, H, n, block_size)
        else:
            idx = rng.integers(0, n, size=(m, H))
        g = np.clip(1.0 + v[idx], 0.0, None)
        path = np.empty((m, H + 1))
        path[:, 0] = initial
        np.cumprod(g, axis=1, out=path[:, 1:])
        path[:, 1:] *= initial
        peak = np.maximum.accumulate(path, axis=1)
        dd = np.where(peak > 0, 1.0 - path / peak, 0.0)
        finals[a:a + m] = path[:, -1]
        mdds[a:a + m] = dd.max(axis=1)
        mins[a:a + m] = path.min(axis=1)
        grid[a:a + m] = path[:, xs]
    return {'finals': finals, 'mdds': mdds, 'mins': mins, 'grid': grid, 'xs': xs, 'horizon': H, 'n': n}


def _pct(a, q) -> Optional[float]:
    a = np.asarray(a, dtype=float)
    return float(np.percentile(a, q)) if len(a) else None


def monte_carlo_from_result(data: Dict[str, Any], request: Dict[str, Any]) -> Dict[str, Any]:
    """Monte Carlo data block from a NORMALISED run_backtest result (`data`)."""
    req = request if isinstance(request, dict) else {}
    raw_method = req.get('mcMethod')
    method = normalize_method(raw_method)
    n_sims = int(max(100, min(int(_f(req.get('mcSimulations')) or 1000), MAX_SIMS)))
    seed = int(_f(req.get('mcSeed')) if _f(req.get('mcSeed')) is not None else 42)
    block = float(_f(req.get('mcBlockSize')) or 20.0)
    ruin_level = float(_f(req.get('mcRuinLevel')) or 0.5)
    eq_rows = [p for p in (data.get('equity') or []) if isinstance(p, dict) and _f(p.get('equity')) is not None]
    equity = np.array([float(p['equity']) for p in eq_rows]) if eq_rows else None
    dates = [str(p.get('date')) for p in eq_rows]
    stats_in = data.get('statistics') or {}
    initial = _f(req.get('initialCapital')) or _f(stats_in.get('initialCapital')) or \
        (float(equity[0]) if equity is not None and len(equity) else 100000.0)
    trades = [t for t in (data.get('trades') or []) if isinstance(t, dict)]
    warnings: List[Dict[str, Any]] = []
    notes: List[str] = []
    position_fraction = None
    trade_pct: List[float] = []
    n_trades = 0
    if method in ('trade_bootstrap', 'trade_shuffle'):
        tr = trade_account_returns(trades, equity, dates, initial)
        vals = tr['returns']
        n_trades = len(vals)
        trade_pct = tr['pnlPercent']
        if n_trades < 3:
            raise ValueError(f'Monte Carlo over trades needs at least 3 trades; the backtest produced {n_trades}. '
                             f'Use the block bootstrap of daily returns (mcMethod "block_bootstrap") instead.')
        position_fraction = float(np.mean(tr['fractions'])) if len(tr['fractions']) else None
        horizon = n_trades
        if tr['nOpen']:
            notes.append(f'{tr["nOpen"]} position(s) still open at the end are included at their marked-to-market P&L.')
        notes.append('Trade resampling compounds each trade on the account at the share of equity it actually used; '
                     'time between trades and overlapping positions are ignored, so drawdowns inside a trade are '
                     'not visible.')
        x_axis_dates = None
    else:
        if equity is None or len(equity) < 31:
            raise ValueError('Monte Carlo over returns needs an equity curve of at least 31 bars.')
        with np.errstate(divide='ignore', invalid='ignore'):
            vals = equity[1:] / equity[:-1] - 1.0
        vals = vals[np.isfinite(vals)]
        horizon = len(vals)
        x_axis_dates = dates
        if method == 'block_bootstrap':
            notes.append(f'Stationary block bootstrap of the {horizon} per-bar returns (mean block {block:g} bars) keeps '
                         f'volatility clustering; serial dependence longer than the blocks is lost.')
        else:
            notes.append('Per-bar returns are drawn independently: volatility clustering and autocorrelation are lost.')
    sim = simulate_paths(method, vals, n_sims=n_sims, seed=seed, initial=initial, block_size=block,
                         horizon=horizon, ruin_level=ruin_level)
    finals, mdds, mins, grid, xs = sim['finals'], sim['mdds'], sim['mins'], sim['grid'], sim['xs']
    orig = initial * np.concatenate([[1.0], np.cumprod(np.clip(1.0 + vals, 0.0, None))])
    opeak = np.maximum.accumulate(orig)
    orig_mdd = float(np.max(np.where(opeak > 0, 1.0 - orig / opeak, 0.0)))
    bt_final = float(equity[-1]) if equity is not None and len(equity) else None
    if bt_final and method.startswith('trade') and abs(orig[-1] / bt_final - 1.0) > 0.005:
        warnings.append({'code': 'MC_NOT_RECONCILED', 'severity': 'warn', 'symbol': None,
                         'message': f'The trades compound to {orig[-1]:,.0f} while the backtest ended at {bt_final:,.0f} '
                                    f'(overlapping positions, cash between trades, or trades missing from the '
                                    f'engine\'s list). Prefer the block bootstrap of returns.'})
    years = None
    if dates and len(dates) > 1:
        try:
            import pandas as pd
            years = (pd.Timestamp(dates[-1][:10]) - pd.Timestamp(dates[0][:10])).days / 365.25
        except Exception:
            years = None
    cagrs = (np.where(finals > 0, (finals / initial) ** (1.0 / years) - 1.0, -1.0) if years and years > 0 else None)
    shuffle = method == 'trade_shuffle'
    band = {q: np.percentile(grid, q, axis=0) for q in (5, 25, 50, 75, 95)}
    rng = np.random.default_rng(seed + 1)
    sample_idx = rng.choice(n_sims, size=min(20, n_sims), replace=False)
    x_vals = [int(x) for x in xs]
    stats = {
        'finalP5': None if shuffle else _pct(finals, 5), 'finalP25': None if shuffle else _pct(finals, 25),
        'finalP50': None if shuffle else _pct(finals, 50), 'finalP75': None if shuffle else _pct(finals, 75),
        'finalP95': None if shuffle else _pct(finals, 95), 'finalMean': None if shuffle else float(np.mean(finals)),
        'originalFinal': float(orig[-1]),
        'originalPercentile': None if shuffle else float(np.mean(finals <= orig[-1])),
        'probLoss': float(np.mean(finals < initial)),
        'probDoubling': float(np.mean(finals >= 2 * initial)),
        'probRuin': float(np.mean(mins <= ruin_level * initial)), 'ruinLevel': ruin_level,
        'ddP50': _pct(mdds, 50), 'ddP95': _pct(mdds, 95), 'ddP99': _pct(mdds, 99),
        'probDdOver20': float(np.mean(mdds > 0.20)),
        'originalMaxDrawdown': orig_mdd,
        'backtestMaxDrawdown': (float(np.max(1.0 - equity / np.maximum.accumulate(equity))) if equity is not None
                                and len(equity) else None),
        'cagrP5': None if (shuffle or cagrs is None) else _pct(cagrs, 5),
        'cagrP50': None if (shuffle or cagrs is None) else _pct(cagrs, 50),
        'cagrP95': None if (shuffle or cagrs is None) else _pct(cagrs, 95),
        'meanTradeReturn': float(np.mean(trade_pct)) if trade_pct else None,
        'meanStepReturn': float(np.mean(vals)) if len(vals) else None,
    }
    perf = data.get('performance') or {}
    thin_sel = min(len(finals), 2000)
    sel = np.linspace(0, len(finals) - 1, thin_sel).astype(int)
    assumptions = dict(data.get('assumptions') or {})
    assumptions['notes'] = list(assumptions.get('notes') or []) + [
        f'Monte Carlo ({method}, {n_sims} paths, seed {seed}). ' + ' '.join(notes)]
    out = {
        'schemaVersion': 2,
        'method': (str(raw_method) if str(raw_method or '').lower() in ('bootstrap', 'reshuffle') else method),
        'methodId': method,
        'nSimulations': n_sims, 'seed': seed,
        'blockSize': block if method == 'block_bootstrap' else None,
        'horizonBars': horizon if method in ('returns_bootstrap', 'block_bootstrap') else None,
        'nTrades': n_trades, 'initialCapital': initial, 'positionFraction': position_fraction,
        'x': x_vals,
        'xDates': ([x_axis_dates[min(i, len(x_axis_dates) - 1)] for i in x_vals] if x_axis_dates else None),
        'bands': [{'lower': 5, 'upper': 95, 'lowerValues': np.round(band[5], 4).tolist(),
                   'upperValues': np.round(band[95], 4).tolist()},
                  {'lower': 25, 'upper': 75, 'lowerValues': np.round(band[25], 4).tolist(),
                   'upperValues': np.round(band[75], 4).tolist()}],
        'percentileBands': {'x': x_vals, **{f'p{q}': np.round(band[q], 4).tolist() for q in (5, 25, 50, 75, 95)}},
        'median': np.round(band[50], 4).tolist(),
        'originalPath': np.round(orig[xs], 4).tolist(),
        'original': np.round(orig[xs], 4).tolist(),
        'samplePaths': [np.round(grid[i], 2).tolist() for i in sample_idx],
        'finalEquity': np.round(finals, 2).tolist(),
        'maxDrawdown': np.round(mdds, 6).tolist(),
        'finalValues': np.round(finals[sel], 2).tolist(),
        'drawdownValues': np.round(mdds[sel], 6).tolist(),
        'tradeReturns': [float(x) for x in trade_pct],
        'stats': stats,
        'backtest': {k: perf.get(k) for k in ('totalReturn', 'annualizedReturn', 'cagr', 'sharpeRatio', 'maxDrawdown',
                                              'winRate', 'totalTrades', 'profitFactor')},
        'assumptions': assumptions,
        'warnings': list(data.get('warnings') or []) + warnings,
        'provenance': data.get('provenance'),
        # decision 7: every result says which engine / implementation produced the resampled run
        'engine': data.get('engine') or (data.get('provenance') or {}).get('engine'),
        'engineImplementation': data.get('engineImplementation') or
        (data.get('provenance') or {}).get('engineImplementation'),
        'usingSyntheticData': bool(data.get('usingSyntheticData')),
        'logs': list(data.get('logs') or []) + [f'Monte Carlo: {n_sims} {method} paths over {len(vals)} '
                                                f'{"trades" if method.startswith("trade") else "bars"}.'],
    }
    return out


def run_monte_carlo(run_backtest: Callable[[Dict[str, Any]], Dict[str, Any]],
                    request: Dict[str, Any]) -> Dict[str, Any]:
    """Legacy entry point: run the provider's (normalised) backtest once, then resample it.
    Returns the envelope {'success': True, 'data': ...} or a failure envelope."""
    from base.base_provider import convert_keys_to_camel
    from base.progress import Progress
    req = dict(request or {})
    try:
        normalize_method(req.get('mcMethod'))
    except ValueError as e:
        return {'success': False, 'error': str(e), 'errorCode': 'INPUT'}
    base = run_backtest(req)
    base = convert_keys_to_camel(base) if isinstance(base, dict) else base
    if not isinstance(base, dict) or not base.get('success', False):
        return base if isinstance(base, dict) else {'success': False, 'error': 'Backtest failed'}
    data = base.get('data') or {}
    if isinstance(data, dict) and data.get('success') is False:
        return {'success': False, 'error': data.get('error') or 'Backtest failed'}
    prog = Progress('monte_carlo', total=None, message='Resampling')
    try:
        out = monte_carlo_from_result(data, req)
    except ValueError as e:
        return {'success': False, 'error': str(e), 'errorCode': 'INPUT'}
    prog.finish(f'{out["nSimulations"]} paths')
    return {'success': True, 'data': out}
