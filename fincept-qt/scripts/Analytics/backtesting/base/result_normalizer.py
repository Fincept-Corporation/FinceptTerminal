"""
Result contract v2 for every backtesting engine (audit Part 6), built in ONE place.

TWO ENTRY POINTS
  normalize_response(command, request, result, ohlc=None, *, identity=None)
      What every provider's main() calls today: takes the engine's own dict (snake or camel case),
      camel-cases FIELD names (data-map keys such as parameter names and symbols are kept), and
      turns run_backtest / optimize / walk_forward / monte_carlo results into contract v2. Never
      raises: a normaliser failure is logged and the result is returned as it was.
  build_backtest_result(request, *, dates, equity, trades=None, fills=None, ...)
      For engines rebased on base/ (simulator, price store): give the curves and lists, get the
      complete v2 `data` dict back (then emit {'success': True, 'data': data} via json_response).

WHAT v2 GUARANTEES (superset of v1: no existing key changes name, type, unit or sign, R5)
  * every headline metric comes from base.metrics (one definition; the engine's own figures are
    kept under engineReported{} for diagnosis): Sharpe / Sortino on excess returns over the
    risk-free rate actually used (a ^IRX T-bill series for USD runs when available, else the
    request's rate - stated in assumptions.riskFreeSource), calendar CAGR, drawdown from the
    running peak with duration in bars and days, alpha / beta on the dates both curves share;
  * statistics.startDate / endDate are the first / last BAR, not the request;
  * trades include positions still open at the end (exitReason 'open', marked to market);
    MAE / MFE use only the part of the entry / exit bars the position was exposed to (B9);
  * costs{} itemises commission, slippage and borrow - a figure the engine did not report is
    null with a warning, never a silent 0;
  * provenance{} (engine, engineImplementation, versions, data source, bars, per-symbol first /
    last bar, cache time), assumptions.fillModel / notModelled, warnings[] with codes;
  * drawdowns[] (top 10 with dates), yearlyReturns, benchmarkMonthlyReturns, periodStats,
    rollingMetrics (correctly dated, 126 bars for daily data, + rollingBeta), exposure{},
    attribution[], positions[], schemaVersion 2;
  * the benchmark is aligned on the dates both series have (no back-fill, B12); its summary
    uses the same metric functions as the strategy (B3); the normaliser loads benchmark and
    OHLC bars through base.price_store (cached; B8) - never its own Yahoo calls.

Fields (types, units, nullability) are listed in base/contract.py, which validates results.
"""

from __future__ import annotations

import math
import os
import platform
import sys
import time
import uuid
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Sequence

from base.base_provider import convert_keys_to_camel, normalize_envelope, _PROCESS_T0

SCHEMA_VERSION = 2
_MAX_PRICE_SYMBOLS = 10
_MAX_PRICE_BARS = 20000
BACKTEST_COMMANDS = ('run_backtest', 'execute_fincept_strategy')

HEADLINE_KEYS = ('totalReturn', 'annualizedReturn', 'cagr', 'volatility', 'sharpeRatio', 'sortinoRatio', 'calmarRatio',
                 'maxDrawdown', 'maxDrawdownDuration', 'maxDrawdownDurationDays', 'downsideDeviation', 'alpha', 'beta',
                 'informationRatio', 'trackingError', 'treynorRatio', 'exposureTime', 'recoveryFactor', 'totalTrades',
                 'winningTrades', 'losingTrades', 'winRate', 'lossRate', 'profitFactor')
# riskFreeSource values meaning "the default yield series for the currency" (^IRX for USD), not a ticker
RF_SOURCE_AUTO_ALIASES = ('auto', 'series', 'yield', 'yield_series', 'tbill', 't-bill', 'default')
NOT_MODELLED = ('bid/ask spread', 'market impact', 'partial fills', 'volume limits', 'borrow cost', 'margin calls',
                'cash interest', 'dividend withholding', 'FX conversion', 'trading halts', 'delisted symbols')


def _log(msg: str) -> None:
    print(f'[normalizer] {msg}', file=sys.stderr)


def _f(v, default=None):
    try:
        x = float(v)
        return x if math.isfinite(x) else default
    except (TypeError, ValueError):
        return default


# ════════════════════════════════════════════════════════════════════════════
# Engine identity (decision 7)
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class EngineIdentity:
    """What the user picked (engine) and what really ran (implementation, library, versions)."""
    engine: str
    implementation: str
    library: Optional[str] = None            # importable distribution name when a library ran
    version: Optional[str] = None            # engine / provider code version
    fill_mode: str = 'next_open'
    commission_itemised: bool = True         # trades / statistics carry the commission paid
    slippage_itemised: bool = False          # slippage money is reported (per trade or in total)
    slippage_note: Optional[str] = None      # how the engine applies slippage when it does not itemise it
    modelled: Sequence[str] = ()             # entries of NOT_MODELLED this engine does model
    trade_mode: str = 'signals'              # 'signals' | 'rebalance' (FIFO fragments)
    notes: List[str] = field(default_factory=list)


# What the audit established each engine to be today (until its batch rebases it; the per-engine
# coders pass their own EngineIdentity to normalize_response / build_backtest_result).
LEGACY_IDENTITIES: Dict[str, EngineIdentity] = {
    'vectorbt': EngineIdentity('vectorbt', 'Fincept vectorbt-style simulator (pure-numpy bar loop; the vectorbt '
                                           'library is not used)', None, fill_mode='next_open',
                               slippage_note='slippage is applied to fill prices but not reported'),
    'backtestingpy': EngineIdentity('backtestingpy', 'backtesting.py library', 'backtesting', fill_mode='next_open',
                                    slippage_note='slippage is added to the commission rate, so it is included in '
                                                  'the commission figures'),
    'bt': EngineIdentity('bt', 'bt library', 'bt', fill_mode='next_close', trade_mode='rebalance',
                         slippage_note='this engine applies no slippage'),
    'zipline': EngineIdentity('zipline', 'Fincept zipline-style event simulator (zipline-reloaded is not used)', None,
                              fill_mode='next_open', commission_itemised=False,
                              slippage_note='slippage is applied to fill prices but not reported'),
    'fincept': EngineIdentity('fincept', 'Fincept bar-loop engine (strategies/fincept_engine)', None,
                              fill_mode='next_open', slippage_itemised=True),
}


def _engine_from_argv() -> Optional[str]:
    try:
        name = os.path.basename(sys.argv[0] or '')
    except Exception:
        return None
    if name.endswith('_provider.py'):
        return name[:-len('_provider.py')]
    return None


def resolve_identity(identity: Optional[EngineIdentity], data: Dict[str, Any]) -> EngineIdentity:
    if identity is not None:
        return identity
    eng = _engine_from_argv() or 'unknown'
    base = LEGACY_IDENTITIES.get(eng)
    if base is None:
        return EngineIdentity(eng, f'{eng} provider', None)
    ident = EngineIdentity(**{k: getattr(base, k) for k in base.__dataclass_fields__})
    eng_text = str((data.get('assumptions') or {}).get('engine') or '')
    if eng == 'bt' and 'numpy' in eng_text.lower():
        ident.implementation = 'Fincept numpy signal model (this strategy does not run on the bt library)'
        ident.library = None
        ident.trade_mode = 'signals'
    return ident


def _dist_version(name: Optional[str]) -> Optional[str]:
    if not name:
        return None
    try:
        from importlib.metadata import version
        return version(name)
    except Exception:
        return None


# ════════════════════════════════════════════════════════════════════════════
# Entry point used by the providers
# ════════════════════════════════════════════════════════════════════════════

def normalize_response(command: str, request: Dict[str, Any], result: Any, ohlc: Optional[Dict[str, Any]] = None,
                       *, identity: Optional[EngineIdentity] = None) -> Any:
    """Camel-case `result` and build contract v2 for backtest / optimise / walk-forward / Monte Carlo.
    Never raises (a normaliser bug must not cost the user the run)."""
    try:
        out = convert_keys_to_camel(result)
    except Exception:
        return result
    if not isinstance(out, dict):
        return out
    out = normalize_envelope(out)
    if out.get('success') is False:
        return out
    data = out.get('data') if isinstance(out.get('data'), dict) else None
    if data is None:
        return out
    req = request if isinstance(request, dict) else {}
    try:
        if command in BACKTEST_COMMANDS:
            normalize_backtest(data, req, ohlc, identity=identity)
        elif command == 'optimize':
            normalize_optimize(data, req, identity=identity)
        elif command == 'walk_forward':
            normalize_walk_forward(data, req, identity=identity)
        elif command == 'monte_carlo':
            data.setdefault('schemaVersion', SCHEMA_VERSION)
    except Exception as e:  # pragma: no cover - defensive
        import traceback
        _log(f'{command} normalisation failed (result returned as is): {e}\n{traceback.format_exc()}')
        logs = data.get('logs')
        if isinstance(logs, list):
            logs.append(f'Result normalisation failed: {e}')
        data.pop('schemaVersion', None)
    return out


# ════════════════════════════════════════════════════════════════════════════
# Market data for the normaliser (benchmark, OHLC, risk-free) through the price store
# ════════════════════════════════════════════════════════════════════════════

def _load_prices(symbols: List[str], start: str, end: str, interval: str, synthetic: bool) -> Dict[str, Any]:
    """{'frames', 'provenance', 'errors', 'networkCalls'} - one batched price-store call."""
    out = {'frames': {}, 'provenance': {}, 'errors': {}, 'networkCalls': 0}
    if synthetic or not symbols:
        return out
    try:
        from base.price_store import get_bars, normalize_interval
        iv = normalize_interval(interval)
        res = get_bars(symbols, start, end, interval=iv, progress=False)
        out.update({'frames': res.frames, 'provenance': res.provenance,
                    'errors': {k: str(v) for k, v in res.errors.items()}, 'networkCalls': res.network_calls})
    except Exception as e:
        _log(f'price store unavailable: {e}')
        out['errors'] = {s: str(e) for s in symbols}
    return out


def _canon(sym) -> str:
    try:
        from base.price_store import normalize_symbol
        return normalize_symbol(sym)
    except Exception:
        return str(sym or '').strip().upper()


# ════════════════════════════════════════════════════════════════════════════
# run_backtest
# ════════════════════════════════════════════════════════════════════════════

def normalize_backtest(data: Dict[str, Any], request: Dict[str, Any], ohlc: Optional[Dict[str, Any]] = None, *,
                       identity: Optional[EngineIdentity] = None, cost_model=None,
                       price_provenance: Optional[Dict[str, Dict[str, Any]]] = None) -> None:
    import numpy as np
    import pandas as pd
    from base import metrics as M
    from base.progress import emit

    emit('metrics', 0, None, 'Computing metrics', force=True)
    ident = resolve_identity(identity, data)
    logs = data.get('logs')
    if not isinstance(logs, list):
        logs = [] if logs in (None, '') else [str(logs)]
    data['logs'] = [str(x) for x in logs]
    warnings: List[Dict[str, Any]] = [w for w in (data.get('warnings') or []) if isinstance(w, dict)]

    def warn(code, message, severity='warn', symbol=None):
        if not any(w.get('code') == code and w.get('symbol') == symbol for w in warnings):
            warnings.append({'code': code, 'severity': severity, 'message': message, 'symbol': symbol})

    perf_in = data.get('performance') if isinstance(data.get('performance'), dict) else {}
    stats = data.get('statistics') if isinstance(data.get('statistics'), dict) else {}
    adv_in = data.get('advancedMetrics') if isinstance(data.get('advancedMetrics'), dict) else {}
    engine_reported = {'performance': dict(perf_in)}
    if adv_in:
        engine_reported['advancedMetrics'] = {k: v for k, v in adv_in.items() if not isinstance(v, (list, dict))}
    synthetic = bool(data.get('usingSyntheticData'))

    # ── equity curve ─────────────────────────────────────────────────────────
    rows = [p for p in (data.get('equity') or []) if isinstance(p, dict)]
    parsed = []
    for p in rows:
        ts = _parse_ts(p.get('date'))
        v = _f(p.get('equity'))
        if ts is not None and v is not None:
            parsed.append((ts, v, p))
    parsed.sort(key=lambda x: x[0])
    dedup = {}
    for ts, v, p in parsed:
        dedup[ts] = (v, p)
    if len(dedup) < 2:
        data['equity'] = [x[2] for x in parsed]
        warn('SHORT_HISTORY', 'The run produced fewer than 2 bars; no metrics can be computed.', 'error')
        data['warnings'] = warnings
        return
    idx = pd.DatetimeIndex(sorted(dedup))
    values = np.array([dedup[t][0] for t in idx], dtype=float)
    keep_rows = [dedup[t][1] for t in idx]
    intraday = bool(((idx.hour != 0) | (idx.minute != 0) | (idx.second != 0)).any())
    dates = [_fmt_ts(t, intraday) for t in idx]
    n = len(values)
    initial = _f(request.get('initialCapital')) or _f(stats.get('initialCapital')) or float(values[0])
    ppy = M.infer_periods_per_year(idx)

    # ── symbols, prices, benchmark, risk-free (one batched price-store call) ──
    req_symbols = [_canon(s) for s in (request.get('symbols') or []) if str(s).strip()]
    trades_in = [t for t in (data.get('trades') or []) if isinstance(t, dict)]
    fills_in = None
    if trades_in and all(not t.get('exitDate') for t in trades_in) and \
            all(str(t.get('side', '')).lower() in ('buy', 'sell') for t in trades_in):
        fills_in = trades_in
    trade_syms = []
    for t in (fills_in or trades_in):
        s = _canon(t.get('symbol'))
        if s and s not in trade_syms:
            trade_syms.append(s)
    symbols = list(req_symbols)
    for s in trade_syms:
        if s not in symbols:
            symbols.append(s)
    bench_sym = _canon(request.get('benchmarkSymbol') or request.get('benchmark') or '')
    rf_source_req = str(request.get('riskFreeSource') or 'auto').strip()
    if rf_source_req.lower() in RF_SOURCE_AUTO_ALIASES:
        rf_source_req = 'auto'   # the screen sends "series" for "the T-bill series" (not a ticker called SERIES)
    frames = _ohlc_frames(ohlc or {})
    need = [s for s in symbols[:_MAX_PRICE_SYMBOLS] if s not in frames]
    have_bench_rows = sum(1 for p in keep_rows if _f(p.get('benchmark')) is not None) >= 0.9 * n
    if bench_sym and not have_bench_rows and bench_sym not in frames and bench_sym not in need:
        need.append(bench_sym)
    from base.price_store import currency_for_symbol
    ccy = None
    first_sym = req_symbols[0] if req_symbols else (trade_syms[0] if trade_syms else None)
    if price_provenance and first_sym in price_provenance:
        ccy = price_provenance[first_sym].get('currency')
    ccy = ccy or (currency_for_symbol(first_sym) if first_sym else None)
    rf_symbol = None
    if rf_source_req.lower() == 'auto' and ccy == 'USD' and not synthetic:
        rf_symbol = '^IRX'
    elif rf_source_req.lower() not in ('auto', 'rate', 'request', '') and not synthetic:
        rf_symbol = _canon(rf_source_req)
    if rf_symbol and rf_symbol not in need:
        need.append(rf_symbol)
    interval = str(request.get('interval') or '1d')
    if intraday and interval in ('1d', '1wk', '1mo'):
        interval = '1h'
    rf_frames_needed = [s for s in need if s == rf_symbol]
    price_needed = [s for s in need if s != rf_symbol]
    loaded = {'frames': {}, 'provenance': {}, 'errors': {}, 'networkCalls': 0}
    if need and not synthetic:
        # Prices and benchmark at the run's interval; the risk-free series is always daily.
        if price_needed:
            loaded = _load_prices(price_needed, dates[0][:10], dates[-1][:10], interval, synthetic)
        if rf_frames_needed:
            rfl = _load_prices(rf_frames_needed, dates[0][:10], dates[-1][:10], '1d', synthetic)
            for k in ('frames', 'provenance', 'errors'):
                loaded[k].update(rfl[k])
            loaded['networkCalls'] += rfl['networkCalls']
    for s, f in loaded['frames'].items():
        frames.setdefault(s, f)
    prov_syms = dict(loaded['provenance'])
    if price_provenance:
        prov_syms.update(price_provenance)

    # Benchmark series (strategy dates; NaN before the benchmark's history starts)
    bench_vals = None
    bench_dates = None
    if have_bench_rows:
        bench_vals = np.array([_f(p.get('benchmark'), np.nan) for p in keep_rows], dtype=float)
        bench_dates = idx
    elif bench_sym and bench_sym in frames:
        bf = frames[bench_sym]['Close'].astype(float)
        bench_dates = pd.DatetimeIndex(bf.index)
        bench_vals = bf.values
    elif bench_sym:
        reason = loaded['errors'].get(bench_sym) or 'not available'
        warn('BENCHMARK_UNAVAILABLE', f'Benchmark {bench_sym} could not be loaded ({reason}); benchmark-relative '
                                      f'metrics are omitted.', 'info', bench_sym)

    # Risk-free
    rf_value = _f(request.get('riskFreeRate'), None)
    rf_used: Any = rf_value if rf_value is not None else 0.0
    rf_source = 'request' if rf_value is not None else 'default (0)'
    rf_desc = f'{(rf_value or 0.0) * 100:.2f}% a year (the rate in the request)' if rf_value is not None else \
        '0% (no rate given)'
    rf_yield = None   # annual yield series (when a series is used), for any other date index (benchmark)
    if rf_symbol and rf_symbol in frames:
        y = frames[rf_symbol]['Close'].astype(float) / 100.0
        y = y[~y.index.duplicated(keep='last')]
        al = y.reindex(y.index.union(idx.normalize())).ffill().reindex(idx.normalize())
        cov = float(np.isfinite(al.values).mean())
        if cov >= 0.9:
            rf_used = pd.Series(al.values).ffill().bfill().to_numpy()
            rf_yield = y
            rf_source = rf_symbol
            mean_rf = float(np.nanmean(rf_used))
            rf_desc = (f'{rf_symbol} {"(13-week US T-bill yield)" if rf_symbol == "^IRX" else "yield series"}, '
                       f'{mean_rf * 100:.2f}% a year on average over the period, accrued per bar')
            if rf_value is not None and abs(rf_value - mean_rf) > 0.0025:
                warn('RISK_FREE_SERIES', f'Sharpe, Sortino and alpha use the {rf_symbol} T-bill series (average '
                                         f'{mean_rf * 100:.2f}%), not the {rf_value * 100:.2f}% entered. Send '
                                         f'riskFreeSource "rate" to use the entered rate.', 'info')
    elif rf_symbol and rf_source_req.lower() not in ('auto',):
        warn('RISK_FREE_UNAVAILABLE', f'The risk-free series {rf_symbol} could not be loaded; the rate in the '
                                      f'request was used.', 'info', rf_symbol)

    # ── headline metrics (one definition, strategy and benchmark alike) ──────
    why: Dict[str, str] = {}
    head = M.headline(values, idx, rf=rf_used, ppy=ppy, bench_equity=bench_vals, bench_dates=bench_dates, why=why)
    rf_bar_all = M.rf_per_bar(n - 1, rf_used, ppy, idx)
    bench_summary = None
    bench_rows = [None] * n
    bench_ret_aligned = None
    if bench_vals is not None:
        bseries = pd.Series(bench_vals, index=bench_dates)
        bseries = bseries[~bseries.index.duplicated(keep='last')]
        bwin = bseries[(bseries.index >= idx[0]) & (bseries.index <= idx[-1])].dropna()
        aligned = bseries.reindex(bseries.index.union(idx)).ffill().reindex(idx)
        common = idx.intersection(bwin.index)
        if len(bwin) >= 2 and len(common) >= 2:
            base_date = common[0]
            base_b = float(bseries.loc[base_date])
            base_e = float(values[idx.get_loc(base_date)])
            started = idx >= base_date
            vals_b = np.where(started, aligned.values / base_b * base_e, np.nan)
            bench_rows = [None if not np.isfinite(x) else float(x) for x in vals_b]
            if rf_yield is None:
                b_rf = rf_used
            else:   # the same risk-free series as the strategy, on the benchmark's own dates (B3)
                bi = bwin.index.normalize()
                b_rf = pd.Series(rf_yield.reindex(rf_yield.index.union(bi)).ffill().reindex(bi).values
                                 ).ffill().bfill().to_numpy()
            bh = M.headline(bwin.values, bwin.index, rf=b_rf, ppy=M.infer_periods_per_year(bwin.index))
            bench_summary = {'symbol': bench_sym or None, 'totalReturn': bh['totalReturn'],
                             'annualizedReturn': bh['cagr'], 'cagr': bh['cagr'], 'volatility': bh['volatility'],
                             'maxDrawdown': bh['maxDrawdown'], 'sharpeRatio': bh['sharpeRatio'],
                             'sortinoRatio': bh['sortinoRatio'], 'alignedBars': int(len(common)),
                             'firstBar': _fmt_ts(bwin.index[0], intraday), 'lastBar': _fmt_ts(bwin.index[-1], intraday)}
            if len(common) < 0.9 * n:
                warn('BENCHMARK_PARTIAL', f'The benchmark {bench_sym or ""} shares {len(common)} of the strategy\'s '
                                          f'{n} bars (different calendars or a later start); relative metrics use '
                                          f'the shared dates only.', 'info', bench_sym or None)
            if bench_dates is idx:
                bench_ret_aligned = M.simple_returns(np.where(np.isfinite(bench_vals), bench_vals, np.nan))
            else:
                bench_ret_aligned = M.simple_returns(aligned.values)
        else:
            warn('BENCHMARK_UNAVAILABLE', f'The benchmark {bench_sym or ""} has no bars in the run\'s period.',
                 'info', bench_sym or None)

    # ── equity rows (rounded; v1 keys + exposure when known) ─────────────────
    dd = M.drawdown_series(values)
    rets = np.zeros(n)
    rets[1:] = np.where(np.isfinite(M.simple_returns(values)), M.simple_returns(values), 0.0)

    # ── trades ───────────────────────────────────────────────────────────────
    date_pos = {d: i for i, d in enumerate(dates)}
    if fills_in is not None:
        data['fills'] = [_fill_row(f, intraday) for f in fills_in]
        trades = _round_trips_from_fills(fills_in)
        data['logs'].append(f'{len(fills_in)} fills paired into {len(trades)} round-trip trades (FIFO).')
    else:
        trades = [dict(t) for t in trades_in]
        if isinstance(data.get('fills'), list):
            data['fills'] = [_fill_row(f, intraday) for f in data['fills'] if isinstance(f, dict)]
    last_close = {}
    for s, f in frames.items():
        try:
            c = f['Close'].astype(float)
            c = c[c.index <= idx[-1] + pd.Timedelta(days=0 if intraday else 1)]
            if len(c):
                last_close[s] = float(c.iloc[-1])
        except Exception:
            pass
    for t in trades:
        _complete_trade(t, date_pos, idx, intraday, last_close)
    closed = [t for t in trades if t.get('exitDate')]
    open_ = [t for t in trades if not t.get('exitDate')]
    closed.sort(key=lambda t: (t.get('entryBar') if t.get('entryBar') is not None else -1, str(t.get('entryDate'))))
    open_.sort(key=lambda t: str(t.get('entryDate')))
    trades = closed + open_
    for i, t in enumerate(trades, 1):
        if not t.get('id'):
            t['id'] = f't{i}'
    # MAE / MFE from the bars the position was exposed to (B9)
    sym_cols = [s for s in symbols if s in frames]
    if trades and sym_cols:
        H, L = _panel_hl(frames, sym_cols, idx)
        col_of = {s: j for j, s in enumerate(sym_cols)}
        phase = 0 if ident.fill_mode == 'next_open' else 2
        for t in trades:
            j = col_of.get(_canon(t.get('symbol')))
            if j is None and len(sym_cols) == 1:
                j = 0
            if j is None:
                continue
            t['_asset'], t['_entryPhase'] = j, phase
            t['_exitPhase'] = phase if t.get('exitDate') else 2
        from base.execution import add_excursions
        add_excursions([t for t in trades if '_asset' in t], H, L)
        for t in trades:
            for kk in ('_asset', '_entryPhase', '_exitPhase'):
                t.pop(kk, None)
    data['trades'] = trades
    tstats = M.trade_stats(trades, initial)
    data['tradeStats'] = tstats

    # ── exposure ─────────────────────────────────────────────────────────────
    expo_gross = _series_from_rows(keep_rows, 'exposure', n)
    expo_net = _series_from_rows(keep_rows, 'netExposure', n)
    cash_rows = _series_from_rows(keep_rows, 'cash', n)
    expo_trades = M.exposure_from_trades(trades, n)
    if expo_gross is None and trades and sym_cols:
        gv, nv = _position_values(trades, frames, sym_cols, idx)
        if gv is not None:
            with np.errstate(divide='ignore', invalid='ignore'):
                expo_gross = np.where(values > 0, gv / values, np.nan)
                if np.any(np.abs(nv - gv) > 1e-9):
                    expo_net = np.where(values > 0, nv / values, np.nan)
    count = expo_trades['count']
    if expo_gross is not None and not trades:
        count = (np.nan_to_num(expo_gross) > 1e-9).astype(int)
    exposure = M.exposure_summary(count, expo_gross, expo_net if expo_net is not None else expo_gross)
    if expo_net is None and expo_gross is not None:
        exposure['longShare'], exposure['shortShare'] = (1.0, 0.0) if not any(
            t.get('side') == 'short' for t in trades) else (exposure['longShare'], exposure['shortShare'])
    data['exposure'] = exposure

    for i, p in enumerate(keep_rows):
        p['date'] = dates[i]
        p['equity'] = round(float(values[i]), 4)
        p['returns'] = float(f'{rets[i]:.10g}')
        p['drawdown'] = round(float(dd[i]), 8)
        p['benchmark'] = None if bench_rows[i] is None else round(bench_rows[i], 4)
        if expo_gross is not None:
            p['exposure'] = None if not np.isfinite(expo_gross[i]) else round(float(expo_gross[i]), 5)
        if expo_net is not None:
            p['netExposure'] = None if not np.isfinite(expo_net[i]) else round(float(expo_net[i]), 5)
        if cash_rows is not None:
            p['cash'] = None if not np.isfinite(cash_rows[i]) else round(float(cash_rows[i]), 4)
    data['equity'] = keep_rows

    # ── performance (base definitions; engine values under engineReported) ───
    perf = dict(perf_in)
    for k in ('totalReturn', 'annualizedReturn', 'cagr', 'volatility', 'sharpeRatio', 'sortinoRatio', 'calmarRatio',
              'maxDrawdown', 'maxDrawdownDuration', 'maxDrawdownDurationDays', 'downsideDeviation', 'alpha', 'beta',
              'informationRatio', 'trackingError', 'treynorRatio', 'recoveryFactor'):
        perf[k] = head.get(k)
    perf['exposureTime'] = exposure['timeInMarket']
    perf['totalTrades'] = tstats['closedTrades']
    perf['winningTrades'] = tstats['winningTrades']
    perf['losingTrades'] = tstats['losingTrades']
    perf['winRate'] = tstats['winRate']
    perf['lossRate'] = (tstats['losingTrades'] / tstats['closedTrades']) if tstats['closedTrades'] else None
    perf['profitFactor'] = tstats['profitFactor']
    for k in ('averageWin', 'averageLoss', 'largestWin', 'largestLoss', 'expectancy', 'averageTradeReturn'):
        if k not in perf:
            perf[k] = None
    # An account that lost more than its capital (equity <= 0: leverage, short options) has a drawdown
    # beyond 100 %; the contract field is a fraction of capital, so it reads 1.0 (everything lost) with
    # the raw depth kept and a NEGATIVE_EQUITY error-level warning saying how far below zero it went.
    neg_equity = bool(np.nanmin(values) <= 0.0)
    if perf.get('maxDrawdown') is not None and perf['maxDrawdown'] > 1.0:
        engine_reported['maxDrawdownRaw'] = perf['maxDrawdown']
        perf['maxDrawdown'] = 1.0
    if neg_equity:
        lo = int(np.nanargmin(values))
        warn('NEGATIVE_EQUITY', f'The account lost more than its capital: equity reached {values[lo]:,.2f} on '
                                f'{dates[lo][:10]}. Drawdowns are shown as 100 %; returns after that bar are '
                                f'undefined.', 'error')
    data['performance'] = perf

    # ── statistics ───────────────────────────────────────────────────────────
    ds = M.day_statistics(values, idx)
    stats.update({'startDate': dates[0], 'endDate': dates[-1], 'initialCapital': initial,
                  'finalCapital': float(values[-1]), 'totalTrades': tstats['closedTrades']})
    stats.update(ds)

    # ── costs ────────────────────────────────────────────────────────────────
    costs = _costs_block(data, trades, values, idx, request, ident, tstats, initial, cost_model, warn)
    data['costs'] = costs
    stats['totalFees'] = costs['totalCommission']
    stats['totalSlippage'] = costs['totalSlippage']
    data['statistics'] = stats

    # ── tables, rolling, risk ───────────────────────────────────────────────
    data['drawdowns'] = [{k: v for k, v in d.items() if k not in ('peakIdx', 'troughIdx', 'recoveryIdx')}
                         for d in M.drawdown_table(values, idx, top=10)]
    for d in data['drawdowns']:
        if _f(d.get('depth')) is not None and d['depth'] > 1.0:   # equity below zero (NEGATIVE_EQUITY above)
            d['depthRaw'] = d['depth']
            d['depth'] = 1.0
    if not isinstance(data.get('drawdownAnalysis'), dict) or not data['drawdownAnalysis']:
        data['drawdownAnalysis'] = M.drawdown_analysis_v1(values, idx)
    data['monthlyReturns'] = M.monthly_table(values, idx)
    data['yearlyReturns'] = M.yearly_table(values, idx, bench_vals, bench_dates)
    if bench_vals is not None and bench_summary is not None:
        bs = pd.Series(bench_vals, index=bench_dates)
        bs = bs[(bs.index >= idx[0]) & (bs.index <= idx[-1])].dropna()
        data['benchmarkMonthlyReturns'] = M.monthly_table(bs.values, bs.index) if len(bs) > 1 else []
    win = M.default_rolling_window(ppy)
    roll = M.rolling_series(values, idx, win, rf_bar=rf_bar_all if not np.isscalar(rf_used) else rf_used, ppy=ppy,
                            bench_returns=bench_ret_aligned)
    roll['window'] = win
    data['rollingMetrics'] = roll
    pstats = M.period_stats(values, idx)
    data['periodStats'] = pstats
    ra = data.get('returnsAnalysis') if isinstance(data.get('returnsAnalysis'), dict) else {}
    if ra:
        engine_reported['returnsAnalysis'] = {k: ra.get(k) for k in ('bestWeek', 'worstWeek', 'bestMonth', 'worstMonth',
                                                                     'longestWinStreak', 'longestLossStreak')}
    for k in ('bestWeek', 'worstWeek', 'bestMonth', 'worstMonth', 'longestWinStreak', 'longestLossStreak'):
        ra[k] = pstats.get(k)
    data['returnsAnalysis'] = ra
    r_all = M.simple_returns(values)
    adv = dict(adv_in)
    adv.update({
        'var95': M.value_at_risk(r_all, 0.95), 'var99': M.value_at_risk(r_all, 0.99),
        'cvar95': M.conditional_var(r_all, 0.95), 'cvar99': M.conditional_var(r_all, 0.99),
        'ulcerIndex': M.ulcer_index(values),
        'ulcerIndexFraction': (M.ulcer_index(values) or 0.0) / 100.0,
        'omegaRatio': M.omega_ratio(r_all, rf_bar_all), 'tailRatio': M.tail_ratio(r_all),
        'skewness': M.skewness(r_all), 'kurtosis': M.excess_kurtosis(r_all),
        'benchmarkAlpha': head.get('alpha'), 'benchmarkBeta': head.get('beta'),
        'benchmarkCorrelation': head.get('correlation'), 'rSquared': head.get('rSquared'),
        'informationRatio': head.get('informationRatio'), 'trackingError': head.get('trackingError'),
        'upCapture': head.get('upCapture'), 'downCapture': head.get('downCapture'),
        'returnsHistogram': M.histogram(r_all, 20),
    })
    rc = r_all[np.isfinite(r_all)]
    if len(rc) > 1:
        adv.update({'avgDailyReturn': float(rc.mean()), 'dailyReturnStd': float(rc.std(ddof=1)),
                    'dailyReturnP5': float(np.percentile(rc, 5)), 'dailyReturnP25': float(np.percentile(rc, 25)),
                    'dailyReturnP75': float(np.percentile(rc, 75)), 'dailyReturnP95': float(np.percentile(rc, 95))})
    for k in ('monthlyReturns', 'rollingSharpe', 'rollingVolatility', 'rollingDrawdown'):
        adv.pop(k, None)
    data['advancedMetrics'] = adv
    if bench_summary:
        data['benchmark'] = bench_summary
    else:
        data.pop('benchmark', None)
    data['periodsPerYear'] = ppy
    ext = data.get('extendedStats') if isinstance(data.get('extendedStats'), dict) else None
    et = _f((ext or {}).get('exposureTime'))
    if et is not None and not (-1e-9 <= et <= 1 + 1e-9) and exposure.get('timeInMarket') is not None:
        # v1 key the screen reads; vectorbt summed it over symbols (2.03 = 203 %): impossible as a share.
        engine_reported.setdefault('extendedStats', {})['exposureTime'] = et
        ext['exposureTime'] = exposure['timeInMarket']

    # ── attribution, positions ───────────────────────────────────────────────
    data['attribution'] = M.attribution(trades, initial, n)
    pos = []
    for t in open_:
        mark = _f(t.get('markPrice'))
        q = _f(t.get('quantity'), 0.0)
        sign = -1.0 if t.get('side') == 'short' else 1.0
        if mark is None:
            continue
        mv = sign * q * mark
        pos.append({'symbol': t.get('symbol'), 'quantity': sign * q, 'avgPrice': _f(t.get('entryPrice')),
                    'lastPrice': mark, 'marketValue': mv, 'weight': mv / values[-1] if values[-1] else None,
                    'unrealizedPnl': _f(t.get('pnl'))})
    data['positions'] = pos

    # ── priceData (trade chart) ──────────────────────────────────────────────
    if 'priceData' not in data or not data.get('priceData'):
        price = {}
        for sym in symbols[:_MAX_PRICE_SYMBOLS]:
            fr = frames.get(sym)
            if fr is None or len(fr) == 0:
                continue
            fr = fr[(fr.index >= idx[0]) & (fr.index <= idx[-1] + pd.Timedelta(days=0 if intraday else 1))]
            if len(fr) > _MAX_PRICE_BARS:
                data['logs'].append(f'Price chart for {sym} omitted: {len(fr)} bars exceeds {_MAX_PRICE_BARS}.')
                continue
            if len(fr):
                price[sym] = _columnar(fr, intraday)
        if price:
            data['priceData'] = price

    # ── warnings ─────────────────────────────────────────────────────────────
    _standard_warnings(data, request, ident, trades, tstats, values, idx, dates, initial, warn, prov_syms, symbols,
                       synthetic, intraday)

    # ── assumptions, provenance, identity ────────────────────────────────────
    a = data.get('assumptions') if isinstance(data.get('assumptions'), dict) else {}
    from base.execution import fill_model_block
    a.setdefault('engine', ident.implementation)
    a.setdefault('execution', fill_model_block(ident.fill_mode)['description'])
    a.setdefault('dataSource', 'Yahoo Finance (split/dividend-adjusted prices)')
    a['riskFreeRate'] = float(head.get('rfAnnual') or 0.0)
    a['riskFreeSource'] = rf_source
    a['riskFreeDescription'] = rf_desc
    a.setdefault('annualization', M.describe_periods_per_year(ppy))
    a['fillModel'] = fill_model_block(ident.fill_mode)
    modelled = set(ident.modelled or ())
    a['notModelled'] = [x for x in NOT_MODELLED if x not in modelled]
    a.setdefault('ignoredSettings', [])
    notes = list(a.get('notes') or [])
    for nte in ident.notes:
        if nte not in notes:
            notes.append(nte)
    if any(w.get('code') == 'CALENDAR_MISMATCH' for w in warnings):
        notes.append('Symbols trading on different calendars are aligned on the union of their session dates; a closed '
                     'market\'s price is carried forward for valuation only.')
    notes.append('Money is in the first symbol\'s trading currency' + (f' ({ccy})' if ccy else '') +
                 '; symbols in other currencies are not converted.')
    a['notes'] = notes
    data['assumptions'] = a
    eng_ver = ident.version or _dist_version(ident.library)
    req_start = str(request.get('startDate') or '')[:10] or None
    req_end = str(request.get('endDate') or '')[:10] or None
    sym_prov = []
    for s in (req_symbols or trade_syms):
        p = prov_syms.get(s) or {}
        sym_prov.append({'symbol': s, 'bars': p.get('bars'), 'firstBar': p.get('firstBar'), 'lastBar': p.get('lastBar'),
                         'requestedStart': req_start, 'currency': p.get('currency') or currency_for_symbol(s),
                         'missingBars': p.get('missingBars'), 'source': p.get('source', 'yahoo'),
                         'cacheAsOf': p.get('cacheAsOf'), 'loadedBy': p.get('loadedBy', 'engine' if identity is None
                                                                          else 'price_store')})
    strat = request.get('strategy') if isinstance(request.get('strategy'), dict) else {}
    run_id = str(uuid.uuid4())
    old_id = data.get('id')
    import re as _re
    if not old_id or str(old_id) == str(strat.get('type') or '') or _re.fullmatch(r'[A-Za-z_]{0,8}-?\d{6,}',
                                                                               str(old_id)):
        if old_id:
            engine_reported['id'] = old_id
        data['id'] = run_id
    data['provenance'] = {
        'engine': ident.engine, 'engineImplementation': ident.implementation, 'engineVersion': eng_ver,
        'library': ident.library, 'libraryVersion': _dist_version(ident.library),
        'pythonVersion': platform.python_version(), 'runId': run_id,
        'startedAt': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime(_PROCESS_T0)),
        'durationMs': int((time.time() - _PROCESS_T0) * 1000),
        'dataSource': ('synthetic' if synthetic else (f"broker:{request.get('brokerDataSource')}"
                                                      if request.get('brokerDataSource') else 'yahoo')),
        'adjustment': 'split+dividend', 'interval': str(request.get('interval') or '1d'), 'bars': n,
        'firstBar': dates[0], 'lastBar': dates[-1], 'requestedStart': req_start, 'requestedEnd': req_end,
        'warmupBars': data.get('warmupBars'), 'seed': request.get('seed'),
        'strategy': {'type': strat.get('type'), 'name': strat.get('name')},
        'parameters': strat.get('params') if isinstance(strat.get('params'), dict) else {},
        'symbols': sym_prov, 'currency': ccy, 'priceStoreCalls': loaded['networkCalls'],
        'schemaVersion': SCHEMA_VERSION,
    }
    data['engine'] = ident.engine
    data['engineImplementation'] = ident.implementation
    data['engineVersion'] = eng_ver
    data['engineReported'] = engine_reported
    data['undefinedMetrics'] = why
    data['warnings'] = warnings
    data['schemaVersion'] = SCHEMA_VERSION
    data['usingSyntheticData'] = synthetic
    data.setdefault('status', 'completed')
    emit('metrics', 1, 1, 'Metrics done', force=True)


def _series_from_rows(rows, key, n):
    import numpy as np
    vals = [_f(p.get(key), float('nan')) for p in rows]
    if not any(math.isfinite(v) for v in vals):
        return None
    return np.array(vals, dtype=float)


def _panel_hl(frames, syms, idx):
    import numpy as np
    H = np.full((len(idx), len(syms)), np.nan)
    L = np.full((len(idx), len(syms)), np.nan)
    for j, s in enumerate(syms):
        f = frames[s]
        f = f[~f.index.duplicated(keep='last')]
        hi = f['High'] if 'High' in f else f['Close']
        lo = f['Low'] if 'Low' in f else f['Close']
        H[:, j] = hi.astype(float).reindex(idx).values
        L[:, j] = lo.astype(float).reindex(idx).values
    return H, L


def _position_values(trades, frames, syms, idx):
    """(gross, net) position value per bar from trades' quantities and closes (legacy engines)."""
    import numpy as np
    n = len(idx)
    gross = np.zeros(n)
    net = np.zeros(n)
    closes = {s: frames[s]['Close'].astype(float).reindex(idx).ffill().values for s in syms}
    ok = False
    for t in trades:
        s = _canon(t.get('symbol'))
        c = closes.get(s) if s in closes else (closes[syms[0]] if len(syms) == 1 else None)
        a = t.get('entryBar')
        if c is None or a is None:
            continue
        b = t.get('exitBar') if t.get('exitDate') else n
        if b is None:
            continue
        q = _f(t.get('quantity'), 0.0) or 0.0
        sign = -1.0 if t.get('side') == 'short' else 1.0
        seg = np.nan_to_num(c[int(a):int(b)]) * q
        gross[int(a):int(b)] += seg
        net[int(a):int(b)] += sign * seg
        ok = True
    return (gross, net) if ok else (None, None)


def _costs_block(data, trades, values, idx, request, ident, tstats, initial, cost_model, warn):
    import numpy as np
    from base import metrics as M
    from base.costs import CostModel
    stats = data.get('statistics') if isinstance(data.get('statistics'), dict) else {}
    given = data.get('costs') if isinstance(data.get('costs'), dict) else {}
    req_comm = _f(request.get('commission'), 0.0) or 0.0
    req_slip = _f(request.get('slippage'), 0.0) or 0.0
    comm_trades = sum(abs(_f(t.get('commission'), 0.0) or 0.0) for t in trades)
    fees_stat = _f(stats.get('totalFees'))
    if _f(given.get('totalCommission')) is not None:
        total_comm = _f(given['totalCommission'])
    elif comm_trades > 0:
        total_comm = comm_trades
    elif fees_stat:
        total_comm = fees_stat
    elif req_comm > 0 and trades:
        total_comm = None
        warn('COMMISSION_NOT_REPORTED', f'A commission of {req_comm * 1e4:.1f} bp was requested but this engine does '
                                        f'not report what it charged.', 'warn')
    else:
        total_comm = 0.0
    slip_trades = [_f(t.get('slippageCost')) for t in trades if _f(t.get('slippageCost')) is not None]
    slip_stat = _f(stats.get('totalSlippage'))
    if _f(given.get('totalSlippage')) is not None:
        total_slip = _f(given['totalSlippage'])
    elif slip_trades and sum(slip_trades) > 0:
        total_slip = float(sum(slip_trades))
    elif slip_stat:
        total_slip = slip_stat
    elif req_slip > 0 and trades:
        if ident.slippage_note and 'no slippage' in ident.slippage_note:
            total_slip = 0.0
        else:
            total_slip = None
            warn('SLIPPAGE_NOT_ITEMISED', f'Slippage of {req_slip * 1e4:.1f} bp per fill was requested; '
                                          f'{ident.slippage_note or "this engine does not report what it cost"}.',
                 'info')
    else:
        total_slip = 0.0
    if total_slip is None:
        for t in trades:
            if _f(t.get('slippageCost')) in (None, 0.0):
                t['slippageCost'] = None
    shorts = any(t.get('side') == 'short' for t in trades)
    borrow_trades = [_f(t.get('borrowCost')) for t in trades if _f(t.get('borrowCost')) is not None]
    if _f(given.get('totalBorrow')) is not None:
        total_borrow = _f(given['totalBorrow'])
    elif borrow_trades:
        total_borrow = float(sum(borrow_trades))
    elif shorts:
        total_borrow = None
        warn('BORROW_NOT_MODELLED', 'Short positions were held but this engine charges no borrow fee.', 'info')
    else:
        total_borrow = 0.0
    known = [x for x in (total_comm, total_slip, total_borrow) if x is not None]
    total_costs = float(sum(known)) if known else None
    all_known = all(x is not None for x in (total_comm, total_slip, total_borrow))
    net_pnl = float(values[-1] - values[0])
    gross_pnl = (net_pnl + total_costs) if (all_known and total_costs is not None) else None
    notional = 0.0
    fills = 0
    by_year: Dict[int, Dict[str, float]] = {}
    for t in trades:
        q = abs(_f(t.get('quantity'), 0.0) or 0.0)
        legs = [(t.get('entryDate'), _f(t.get('entryPrice')))]
        if t.get('exitDate'):
            legs.append((t.get('exitDate'), _f(t.get('exitPrice'))))
        c = abs(_f(t.get('commission'), 0.0) or 0.0)
        sl = _f(t.get('slippageCost'))
        for d, px in legs:
            if px is None or not d:
                continue
            fills += 1
            v = q * px
            notional += v
            y = int(str(d)[:4])
            row = by_year.setdefault(y, {'year': y, 'commission': 0.0, 'slippage': 0.0 if total_slip is not None else None,
                                         'tradedNotional': 0.0})
            row['commission'] += c / len(legs)
            if row['slippage'] is not None and sl is not None:
                row['slippage'] += sl / len(legs)
            row['tradedNotional'] += v
    if isinstance(given.get('fills'), (int, float)):
        fills = int(given['fills'])
    if _f(given.get('tradedNotional')) is not None:
        notional = _f(given['tradedNotional'])
    import pandas as pd
    eq = pd.Series(values, index=idx)
    for y, row in by_year.items():
        ey = eq[eq.index.year == y]
        row['turnover'] = (row['tradedNotional'] / float(ey.mean())) if len(ey) and ey.mean() > 0 else None
    model = cost_model
    if model is None:
        try:
            model = CostModel.from_request(request)
        except Exception:
            model = None
    model_d = model.to_contract() if model is not None else None
    if model_d is not None and ident.slippage_note and 'no slippage' in ident.slippage_note:
        model_d['slippageType'] = 'none'
        model_d['slippageValue'] = 0.0
    gp = tstats.get('grossProfit') or 0.0
    out = {
        'totalCommission': total_comm, 'totalSlippage': total_slip, 'totalBorrow': total_borrow,
        'totalCosts': total_costs, 'grossPnl': gross_pnl, 'netPnl': net_pnl,
        'costDrag': M.cost_drag(values, idx, total_costs) if all_known else None,
        'tradedNotional': float(notional), 'annualTurnover': M.annual_turnover(notional, values, idx),
        'fills': int(fills),
        'costsPctOfGrossProfit': (total_costs / gp) if (total_costs is not None and gp > 0) else None,
        'byYear': [by_year[y] for y in sorted(by_year)], 'model': model_d,
        'itemised': {'commission': total_comm is not None, 'slippage': total_slip is not None,
                     'borrow': total_borrow is not None},
    }
    for k in ('cashInterest', 'financingPaid', 'dividends', 'cappedFills', 'sensitivity', 'breakEvenCostBps'):
        if k in given:
            out[k] = given[k]
    if isinstance(given.get('byYear'), list):
        # from the fills (base.costs.costs_by_year): exact when a trade holds many fills (episodes)
        out['byYear'] = given['byYear']
    return out


def _standard_warnings(data, request, ident, trades, tstats, values, idx, dates, initial, warn, prov_syms, symbols,
                       synthetic, intraday):
    import pandas as pd
    a = data.get('assumptions') if isinstance(data.get('assumptions'), dict) else {}
    if synthetic:
        warn('SYNTHETIC_DATA', 'This run used SYNTHETIC prices (random walk), not market data.', 'error')
    for s in (a.get('ignoredSettings') or []):
        warn('IGNORED_SETTING', f'Setting not applied by this engine: {s}.', 'warn', None)
    req_start = _parse_ts(request.get('startDate'))
    req_end = _parse_ts(request.get('endDate'))
    if req_start is not None and (idx[0] - req_start).days > 7:
        first_avail = None
        for s in symbols[:1]:
            p = prov_syms.get(s) or {}
            first_avail = p.get('firstAvailable') if p.get('listingKnown') else None
        warn('START_AFTER_REQUEST', f'The data starts {dates[0][:10]}, not {str(request.get("startDate"))[:10]}'
                                    + (f' (the symbol\'s history begins {first_avail})' if first_avail else '') +
                                    '; returns are annualised over the shorter period.', 'warn')
    if req_end is not None and (req_end - idx[-1]).days > 7:
        warn('END_BEFORE_REQUEST', f'The data ends {dates[-1][:10]}, before the requested '
                                   f'{str(request.get("endDate"))[:10]}.', 'warn')
    strat = request.get('strategy') if isinstance(request.get('strategy'), dict) else {}
    params = strat.get('params') if isinstance(strat.get('params'), dict) else {}
    look = [int(v) for k, v in params.items() if isinstance(v, (int, float)) and not isinstance(v, bool)
            and any(w in k.lower() for w in ('period', 'window', 'lookback', 'slow', 'long', 'length'))
            and 1 < v < 10000 and float(v).is_integer()]
    if look and len(values) < 2 * max(look):
        warn('SHORT_HISTORY', f'{len(values)} bars is short for a {max(look)}-bar lookback (at least '
                              f'{2 * max(look)} recommended); the warm-up leaves few bars to trade.', 'warn')
    pairs = (('fastPeriod', 'slowPeriod'), ('fast_period', 'slow_period'), ('shortWindow', 'longWindow'),
             ('shortPeriod', 'longPeriod'), ('fast', 'slow'))
    for x, y in pairs:
        if isinstance(params.get(x), (int, float)) and isinstance(params.get(y), (int, float)) and params[x] >= params[y]:
            warn('PARAMETER_ORDER', f'{x} ({params[x]}) is not shorter than {y} ({params[y]}); the strategy runs '
                                    f'inverted.', 'info')
    closed = tstats['closedTrades']
    if closed == 0 and tstats['openTrades'] == 0:
        warn('ZERO_TRADES', 'The strategy made no trades in this period; trade statistics are undefined.', 'warn')
    elif 0 < closed < 30:
        warn('FEW_TRADES', f'Only {closed} closed trades: win rate and profit factor are not statistically '
                           f'meaningful.', 'info')
    opens = [t for t in trades if not t.get('exitDate')]
    if opens:
        upnl = sum(_f(t.get('pnl'), 0.0) or 0.0 for t in opens)
        warn('OPEN_POSITION_AT_END', f'{len(opens)} position(s) still open at the last bar, marked to market '
                                     f'({upnl:,.2f} unrealised); they are listed as open trades.', 'info')
    if (_f(request.get('commission'), 0.0) or 0.0) == 0 and (_f(request.get('slippage'), 0.0) or 0.0) == 0 \
            and not request.get('costModel') and not request.get('costPreset'):
        warn('NO_COSTS', 'No commission or slippage was charged: real trading costs would lower these results.',
             'warn')
    pnl_sum = sum(_f(t.get('pnl'), 0.0) or 0.0 for t in trades)
    change = float(values[-1] - values[0])
    if trades and ident.trade_mode == 'signals' and abs(pnl_sum - change) > max(0.005 * initial, 1.0):
        warn('TRADE_LIST_INCOMPLETE', f'The trades add up to {pnl_sum:,.2f} while equity changed by {change:,.2f}; '
                                      f'the engine\'s trade list misses part of the P&L (for example a position still '
                                      f'open at the end).', 'warn')
    if ident.trade_mode == 'rebalance' and trades:
        warn('REBALANCE_TRADE_STATS', 'Trades of a rebalanced portfolio are FIFO fragments of position changes; win '
                                      'rate and profit factor describe those fragments, not strategy decisions.', 'info')
    if ident.fill_mode == 'same_close':
        warn('SAME_BAR_FILL', 'Orders fill at the close of the bar that produced the signal (requested explicitly).',
             'warn')
    req_syms = [_canon(s) for s in (request.get('symbols') or []) if str(s).strip()]
    traded = {_canon(t.get('symbol')) for t in trades if t.get('symbol')}
    if len(req_syms) > 1 and traded and len(traded) < len(req_syms) and \
            any('symbol' in str(s).lower() for s in (a.get('ignoredSettings') or [])):
        warn('IGNORED_SYMBOLS', f'Only {", ".join(sorted(traded))} traded; the engine used one symbol of '
                                f'{len(req_syms)}.', 'warn')
    from base.price_store import currency_for_symbol
    ccys = {currency_for_symbol(s) for s in req_syms}
    ccys.discard(None)
    if len(ccys) > 1:
        warn('MIXED_CURRENCY', f'The symbols trade in {", ".join(sorted(ccys))}; amounts are added without FX '
                               f'conversion.', 'warn')
    for s in req_syms:
        p = prov_syms.get(s) or {}
        if p.get('lastBarProvisional'):
            warn('PARTIAL_LAST_BAR', f'The last bar of {s} ({p.get("lastBar")}) was loaded before its session was '
                                     f'over and may change.', 'info', s)
        if p.get('stale'):
            warn('STALE_DATA', f'{s}: could not refresh the cached prices ({p.get("staleReason")}); using data as of '
                               f'{p.get("cacheAsOf")}.', 'warn', s)


# ── trades ──────────────────────────────────────────────────────────────────

def _fill_row(f, intraday):
    """Contract fill row from an engine fill (bt: entryDate/side/entryPrice; fincept: time/signed
    quantity/price/slippage money); extra engine keys are kept."""
    ts = _parse_ts(f.get('date') or f.get('entryDate') or f.get('time') or f.get('timestamp'))
    q_signed = _f(f.get('quantity'), 0.0) or 0.0
    side = str(f.get('side') or '').lower()
    if side not in ('buy', 'sell'):
        side = 'buy' if q_signed > 0 else 'sell'
    slip = f.get('slippageCost') if f.get('slippageCost') is not None else f.get('slippage')
    out = {k: v for k, v in f.items() if k not in ('time', 'timestamp', 'entryDate', 'entryPrice', 'slippage')}
    out.update({'date': _fmt_ts(ts, intraday) if ts is not None else None, 'symbol': f.get('symbol'), 'side': side,
                'quantity': abs(q_signed),
                'price': _f(f.get('entryPrice') if f.get('entryPrice') is not None else f.get('price')),
                'commission': abs(_f(f.get('commission'), 0.0) or 0.0),
                'slippageCost': None if _f(slip) is None else abs(_f(slip))})
    return out


_EXIT_ALIASES = {'stop': 'stop_loss', 'sl': 'stop_loss', 'stoploss': 'stop_loss', 'stop_loss': 'stop_loss',
                 'tp': 'take_profit', 'takeprofit': 'take_profit', 'take_profit': 'take_profit', 'target': 'take_profit',
                 'trailing': 'trailing_stop', 'trailing_stop': 'trailing_stop', 'trail': 'trailing_stop',
                 'end_of_data': 'end_of_data', 'end': 'end_of_data', 'eod': 'end_of_data', 'liquidated': 'end_of_data',
                 'final': 'end_of_data', 'rebalance': 'rebalance', 'signal': 'signal', 'exit': 'signal',
                 'time_limit': 'time_limit', 'time': 'time_limit', 'margin_call': 'margin_call', 'open': 'open'}


def _norm_exit_reason(t) -> None:
    r = str(t.get('exitReason') or '').strip()
    key = r.lower().replace(' ', '_').replace('-', '_')
    if key in ('open_at_end', 'open_at_close', 'still_open', 'unrealized', 'unrealised'):
        # The engine marked an open position to market and reported it as an exit: list it as open.
        mark = _f(t.get('exitPrice'))
        if mark is not None and _f(t.get('markPrice')) is None:
            t['markPrice'] = mark
        t['exitDate'] = None
        t['exitPrice'] = None
        t['exitBar'] = None
        t['exitReason'] = 'open'
        return
    mapped = _EXIT_ALIASES.get(key)
    if mapped is None and r:
        t['exitReasonDetail'] = r
        mapped = 'signal'
    if mapped:
        t['exitReason'] = mapped


def _round_trips_from_fills(fills):
    """Pair buy/sell fills per symbol (FIFO) into long and short round trips; the open remainder
    becomes open trades (exitDate None, exitReason 'open')."""
    out = []
    book = {}
    for f in sorted(fills, key=lambda x: str(x.get('entryDate') or x.get('date') or '')):
        sym = str(f.get('symbol') or '')
        side = 1 if str(f.get('side', '')).lower() == 'buy' else -1
        qty = abs(_f(f.get('quantity'), 0.0) or 0.0)
        px = _f(f.get('entryPrice') if f.get('entryPrice') is not None else f.get('price'), 0.0) or 0.0
        date = f.get('entryDate') or f.get('date')
        comm = abs(_f(f.get('commission'), 0.0) or 0.0)
        cpu = comm / qty if qty > 0 else 0.0
        lots = book.setdefault(sym, [])
        while qty > 1e-12 and lots and (lots[0][0] > 0) != (side > 0):
            lot = lots[0]
            take = min(qty, abs(lot[0]))
            long_lot = lot[0] > 0
            pnl = (px - lot[1]) * take * (1 if long_lot else -1) - (lot[3] + cpu) * take
            basis = lot[1] * take
            out.append({'symbol': sym, 'side': 'long' if long_lot else 'short', 'entryDate': lot[2], 'exitDate': date,
                        'quantity': take, 'entryPrice': lot[1], 'exitPrice': px, 'commission': (lot[3] + cpu) * take,
                        'pnl': pnl, 'pnlPercent': pnl / basis if basis > 0 else None,
                        'exitReason': str(f.get('exitReason') or 'rebalance')})
            lot[0] = lot[0] - take if long_lot else lot[0] + take
            qty -= take
            if abs(lot[0]) <= 1e-12:
                lots.pop(0)
        if qty > 1e-12:
            lots.append([qty * side, px, date, cpu])
    for sym, lots in book.items():
        for lot in lots:
            out.append({'symbol': sym, 'side': 'long' if lot[0] > 0 else 'short', 'entryDate': lot[2], 'exitDate': None,
                        'quantity': abs(lot[0]), 'entryPrice': lot[1], 'exitPrice': None,
                        'commission': lot[3] * abs(lot[0]), 'pnl': None, 'pnlPercent': None, 'exitReason': 'open'})
    return out


def _bar_index(ts, date_pos, idx, intraday):
    if ts is None:
        return None
    key = _fmt_ts(ts, intraday)
    if key in date_pos:
        return date_pos[key]
    i = int(idx.searchsorted(ts, side='left'))
    return min(i, len(idx) - 1)


def _complete_trade(t, date_pos, idx, intraday, last_close):
    _norm_exit_reason(t)
    side = str(t.get('side') or 'long').lower()
    t['side'] = 'short' if side in ('short', 'sell') else 'long'
    ets = _parse_ts(t.get('entryDate'))
    xts = _parse_ts(t.get('exitDate'))
    if ets is not None:
        t['entryDate'] = _fmt_ts(ets, intraday)
    if xts is not None:
        t['exitDate'] = _fmt_ts(xts, intraday)
    else:
        t['exitDate'] = None
    entry = _f(t.get('entryPrice'))
    exitp = _f(t.get('exitPrice'))
    qty = abs(_f(t.get('quantity'), 0.0) or 0.0)
    t['quantity'] = qty
    sign = -1.0 if t['side'] == 'short' else 1.0
    comm = abs(_f(t.get('commission'), 0.0) or 0.0)
    t['commission'] = comm
    t['entryBar'] = _bar_index(ets, date_pos, idx, intraday)
    t['exitBar'] = _bar_index(xts, date_pos, idx, intraday) if xts is not None else None
    if t['exitDate']:
        if _f(t.get('pnl')) is None and entry is not None and exitp is not None:
            t['pnl'] = (exitp - entry) * qty * sign - comm
        if not t.get('exitReason') or t.get('exitReason') == 'open':
            t['exitReason'] = 'signal'
    else:
        t['exitReason'] = 'open'
        t['exitPrice'] = None
        mark = _f(t.get('markPrice'))
        if mark is None:
            mark = last_close.get(_canon(t.get('symbol')))
            if mark is None and len(last_close) == 1:
                mark = next(iter(last_close.values()))
        if mark is not None:
            t['markPrice'] = mark
            if _f(t.get('pnl')) is None and entry is not None:
                t['pnl'] = (mark - entry) * qty * sign - comm
        t['unrealized'] = True
    pnl = _f(t.get('pnl'))
    if pnl is not None and entry and qty:
        t['pnlPercent'] = pnl / (entry * qty)
    if t['entryBar'] is not None:
        end = t['exitBar'] if t['exitBar'] is not None else len(idx) - 1
        t['holdingPeriod'] = max(0, end - t['entryBar'])
        if ets is not None:
            end_ts = xts if xts is not None else idx[-1]
            t['holdingDays'] = max(0, int((end_ts - ets).days))
    if 'slippageCost' not in t:
        s = _f(t.get('slippage'))
        t['slippageCost'] = s if (s is not None and s > 0) else None
    sc = _f(t.get('slippageCost'))
    if pnl is not None and sc is not None:
        t['grossPnl'] = pnl + comm + sc + (_f(t.get('borrowCost'), 0.0) or 0.0) - (_f(t.get('dividends'), 0.0) or 0.0)
    else:
        t.setdefault('grossPnl', None)


# ── data helpers ────────────────────────────────────────────────────────────

def _parse_ts(v):
    import pandas as pd
    if v is None or v == '':
        return None
    try:
        ts = pd.Timestamp(str(v).replace('Z', ''))
        if ts.tzinfo is not None:
            ts = ts.tz_localize(None)
        return ts
    except Exception:
        return None


def _fmt_ts(ts, intraday):
    return ts.strftime('%Y-%m-%d %H:%M:%S') if intraday else ts.strftime('%Y-%m-%d')


def _ohlc_frames(ohlc):
    """{canonical symbol: DataFrame[Open, High, Low, Close, Volume]} with a naive DatetimeIndex."""
    import pandas as pd
    out = {}
    for key, frame in (ohlc or {}).items():
        if frame is None or not hasattr(frame, 'columns') or 'Close' not in frame.columns:
            continue
        f = frame.copy()
        idx = pd.DatetimeIndex(f.index)
        if idx.tz is not None:
            idx = idx.tz_localize(None)
        f.index = idx
        f = f[~f.index.duplicated(keep='last')].sort_index()
        out[_canon(key)] = f
    return out


def _columnar(frame, intraday):
    import numpy as np

    def col(name):
        if name not in frame.columns:
            return None
        return [None if not np.isfinite(x) else round(float(x), 6) for x in frame[name].astype(float).values]
    return {'t': [_fmt_ts(t, intraday) for t in frame.index],
            'o': col('Open'), 'h': col('High'), 'l': col('Low'), 'c': col('Close'), 'v': col('Volume')}


# ════════════════════════════════════════════════════════════════════════════
# Builder for engines rebased on base/
# ════════════════════════════════════════════════════════════════════════════

def build_backtest_result(request: Dict[str, Any], *, dates, equity, identity: EngineIdentity,
                          trades: Optional[List[Dict[str, Any]]] = None, fills: Optional[List[Dict[str, Any]]] = None,
                          exposure=None, net_exposure=None, cash=None, costs: Optional[Dict[str, Any]] = None,
                          cost_model=None, frames: Optional[Dict[str, Any]] = None,
                          price_provenance: Optional[Dict[str, Dict[str, Any]]] = None,
                          assumptions: Optional[Dict[str, Any]] = None, warnings: Optional[List[Dict]] = None,
                          logs: Optional[List[str]] = None, warmup_bars: Optional[int] = None,
                          using_synthetic: bool = False, extra: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """Complete contract v2 `data` for run_backtest from curves and lists (see module doc).

    dates / equity: one value per bar (cash + positions at the close). trades: contract trade
    dicts (base.simulator SimResult.trades() returns them), open positions included. fills:
    contract fill rows. exposure / net_exposure / cash: per-bar arrays (or None). costs: totals
    the engine itemised ({'totalCommission', 'totalSlippage', 'totalBorrow', 'cashInterest',
    'tradedNotional', 'fills', ...}; SimResult.costs_summary()). frames: {symbol: OHLCV} the
    engine traded (used for MAE / MFE, positions and priceData - no second download).
    price_provenance: BarsResult.provenance."""
    import numpy as np
    from base.metrics import to_datetime_index
    idx = to_datetime_index(dates)
    e = np.asarray(equity, dtype=float)
    intraday = bool(((idx.hour != 0) | (idx.minute != 0) | (idx.second != 0)).any())
    rows = []
    for i in range(len(e)):
        r = {'date': _fmt_ts(idx[i], intraday), 'equity': float(e[i])}
        if exposure is not None:
            r['exposure'] = _f(exposure[i])
        if net_exposure is not None:
            r['netExposure'] = _f(net_exposure[i])
        if cash is not None:
            r['cash'] = _f(cash[i])
        rows.append(r)
    data: Dict[str, Any] = {
        'id': str(uuid.uuid4()), 'status': 'completed', 'equity': rows, 'trades': [dict(t) for t in (trades or [])],
        'statistics': {'initialCapital': _f(request.get('initialCapital')) or float(e[0])},
        'performance': {}, 'assumptions': dict(assumptions or {}), 'warnings': list(warnings or []),
        'logs': list(logs or []), 'usingSyntheticData': bool(using_synthetic), 'warmupBars': warmup_bars,
    }
    if fills is not None:
        data['fills'] = fills
    if costs:
        data['costs'] = dict(costs)
    if extra:
        data.update(extra)
    if price_provenance:
        price_provenance = {_canon(k): dict(v, loadedBy='price_store') for k, v in price_provenance.items()}
    normalize_backtest(data, request, frames, identity=identity, cost_model=cost_model,
                       price_provenance=price_provenance)
    return data


# ════════════════════════════════════════════════════════════════════════════
# Optimisation (legacy engine output)
# ════════════════════════════════════════════════════════════════════════════

def normalize_optimize(data: Dict[str, Any], request: Dict[str, Any], *,
                       identity: Optional[EngineIdentity] = None) -> None:
    """allResults -> {parameters, objectiveValue, performance} sorted best first; sensitivity /
    spread / plateau; the naive 'statisticalSignificance' (B1) replaced by deflated-Sharpe based
    figures from what the engine returned (per-trial Sharpe ratios; normal returns assumed and the
    bar count estimated when the engine gives no return series - PBO then needs the engine to
    optimise through base.robustness and is null with a reason)."""
    import numpy as np
    from base import robustness as R
    ident = resolve_identity(identity, data)
    rows = data.get('allResults') or []
    norm = []
    for r in rows:
        if not isinstance(r, dict):
            continue
        params = r.get('parameters') or r.get('params') or {}
        obj = r.get('objectiveValue', r.get('objective', r.get('value')))
        if isinstance(r.get('performance'), dict):
            perf = r['performance']
        else:
            perf = {k: v for k, v in r.items() if k not in ('parameters', 'params', 'objectiveValue', 'objective', 'value')
                    and not isinstance(v, (dict, list))}
        if _f(obj) is None or not isinstance(params, dict):
            continue
        norm.append({'parameters': params, 'objectiveValue': _f(obj), 'performance': perf})
    norm.sort(key=lambda x: x['objectiveValue'], reverse=True)
    data['allResults'] = norm
    data['schemaVersion'] = SCHEMA_VERSION
    data.setdefault('warnings', [])
    if not norm:
        return
    names = []
    for r in norm:
        for k in r['parameters']:
            if k not in names:
                names.append(k)
    varying = [k for k in names if len({repr(r['parameters'].get(k)) for r in norm}) > 1]
    data['paramNames'] = varying or names
    sens, spread = R._sensitivity(norm, names)
    if not data.get('parameterSensitivity'):
        data['parameterSensitivity'] = sens
    data['parameterSpread'] = spread
    grid = None
    try:
        _, vals = R.param_grid(request.get('paramRanges') or {})
        grid = {k: v for k, v in zip(list((request.get('paramRanges') or {}).keys()), vals)}
        data.setdefault('paramGrid', grid)
    except Exception:
        grid = {k: sorted({repr(r['parameters'].get(k)) for r in norm}) for k in names}
    data.setdefault('bestParameters', norm[0]['parameters'])
    if _f(data.get('bestObjectiveValue')) is None:
        data['bestObjectiveValue'] = norm[0]['objectiveValue']
    data.setdefault('iterations', len(norm))
    data.setdefault('totalCombinations', len(norm))
    data.setdefault('method', str(request.get('optimizeMethod') or 'grid'))
    data.setdefault('objective', str(request.get('optimizeObjective') or 'sharpe'))
    if grid and isinstance(next(iter(grid.values()), [None])[0] if grid else None, (int, float)):
        data['stability'] = R.plateau_score(norm, data['bestParameters'], grid)
    # Overfitting from per-trial Sharpe ratios (approximate: normal returns, bars from the dates)
    ppy = 252.0
    srs = [_f((r.get('performance') or {}).get('sharpeRatio')) for r in norm]
    obj_name = str(data.get('objective') or request.get('optimizeObjective') or '').lower()
    if sum(x is not None for x in srs) < 0.8 * len(norm) and obj_name.startswith('sharpe'):
        srs = [r['objectiveValue'] for r in norm]
    srs_ok = np.array([x for x in srs if x is not None], dtype=float)
    n_trials = int(_f(data.get('iterations')) or len(norm))
    T = _estimate_bars(request, ppy)
    over = {'nTrials': n_trials, 'sharpeStd': None, 'expectedMaxSharpe': None, 'deflatedSharpe': None, 'pbo': None,
            'pboReason': 'this engine does not return per-combination return series (needed for CSCV); it becomes '
                         'available when the engine optimises through base.robustness',
            'effectiveTrials': None, 'approximate': True,
            'method': 'Deflated Sharpe Ratio (Bailey & Lopez de Prado 2014) from the per-trial Sharpe ratios; normal '
                      'returns assumed and the bar count estimated from the dates'}
    if len(srs_ok) >= 2 and T and T > 10:
        per_bar = srs_ok / math.sqrt(ppy)
        var = float(np.var(per_bar, ddof=1))
        sr0 = R.expected_max_sharpe(n_trials, var)
        best = float(per_bar.max())
        den = 1.0 + 0.5 * best * best
        z = (best - sr0) * math.sqrt(T - 1) / math.sqrt(den)
        from statistics import NormalDist
        dsr = float(NormalDist().cdf(z))
        over.update({'sharpeStd': float(np.std(srs_ok, ddof=1)), 'expectedMaxSharpe': sr0 * math.sqrt(ppy),
                     'bestSharpe': float(srs_ok.max()), 'deflatedSharpe': dsr, 'deflatedSharpeZ': z, 'bars': T})
    data['overfitting'] = over
    if data.get('statisticalSignificance'):
        data.setdefault('engineReported', {})['statisticalSignificance'] = data['statisticalSignificance']
    sig = R.significance_block(over, [r['objectiveValue'] for r in norm]) if over['deflatedSharpe'] is not None else None
    if sig:
        data['statisticalSignificance'] = sig
    else:
        data.pop('statisticalSignificance', None)
    data['engine'] = ident.engine
    data['engineImplementation'] = ident.implementation
    data.setdefault('provenance', {'engine': ident.engine, 'engineImplementation': ident.implementation,
                                   'library': ident.library, 'libraryVersion': _dist_version(ident.library),
                                   'requestedStart': str(request.get('startDate') or '')[:10] or None,
                                   'requestedEnd': str(request.get('endDate') or '')[:10] or None,
                                   'seed': request.get('seed'), 'schemaVersion': SCHEMA_VERSION})


def _estimate_bars(request, ppy) -> Optional[int]:
    try:
        import pandas as pd
        a = pd.Timestamp(str(request.get('startDate'))[:10])
        b = pd.Timestamp(str(request.get('endDate'))[:10])
        return int(max(0, (b - a).days) / 365.25 * ppy)
    except Exception:
        return None


# ════════════════════════════════════════════════════════════════════════════
# Walk-forward (legacy engine output)
# ════════════════════════════════════════════════════════════════════════════

def normalize_walk_forward(data: Dict[str, Any], request: Dict[str, Any], *,
                           identity: Optional[EngineIdentity] = None) -> None:
    """Unify window keys; degradationRatio = mean OOS Sharpe / mean IS Sharpe and
    walkForwardEfficiency on ANNUALISED returns (B10: the v1 ratio compared total returns of
    windows of different length); compounded OOS return only when the test windows are
    contiguous; sample std; engine figures kept under engineReported."""
    import numpy as np
    import pandas as pd
    ident = resolve_identity(identity, data)
    windows = [w for w in (data.get('windows') or []) if isinstance(w, dict)]
    warnings = list(data.get('warnings') or [])
    for i, w in enumerate(windows):
        w.setdefault('window', i + 1)
        if 'bestParams' not in w:
            for k in ('bestParameters', 'params', 'parameters'):
                if isinstance(w.get(k), dict):
                    w['bestParams'] = w[k]
                    break
        for src, dst in (('testReturn', 'outOfSampleReturn'), ('testSharpe', 'outOfSampleSharpe'),
                         ('trainReturn', 'inSampleReturn'), ('trainSharpe', 'inSampleSharpe')):
            if dst not in w and src in w:
                w[dst] = w[src]
        for k in ('trainStart', 'trainEnd', 'testStart', 'testEnd'):
            v = w.get(k)
            if isinstance(v, str) and v.endswith(' 00:00:00'):
                w[k] = v[:-9]
    data['windows'] = windows
    data['schemaVersion'] = SCHEMA_VERSION
    rep = data.setdefault('engineReported', {})
    for k in ('degradationRatio', 'oosReturnStd', 'compoundedOosReturn', 'robustnessScore'):
        if k in data:
            rep[k] = data[k]
    real = any(isinstance(w.get('bestParams'), dict) and w.get('bestParams') for w in windows) and \
        any(_f(w.get('inSampleSharpe')) not in (None, 0.0) or _f(w.get('inSampleReturn')) not in (None, 0.0)
            for w in windows)
    if not real:
        for w in windows:
            for k in ('inSampleReturn', 'inSampleSharpe'):
                if _f(w.get(k)) == 0.0:
                    w[k] = None
        warnings.append({'code': 'WALK_FORWARD_NOT_OPTIMISED', 'severity': 'warn', 'symbol': None,
                         'message': 'This engine runs the fixed parameters on sub-periods; it does not optimise on '
                                    'training windows, so in-sample figures and degradation are not available.'})
    oos = [_f(w.get('outOfSampleReturn')) for w in windows if _f(w.get('outOfSampleReturn')) is not None]
    if oos:
        data['nWindows'] = len(windows)
        data['avgOosReturn'] = float(np.mean(oos))
        data['oosReturnStd'] = float(np.std(oos, ddof=1)) if len(oos) > 1 else None
        data['robustnessScore'] = float(np.mean([1.0 if r > 0 else 0.0 for r in oos]))
        sh = [_f(w.get('outOfSampleSharpe')) for w in windows if _f(w.get('outOfSampleSharpe')) is not None]
        data['avgOosSharpe'] = float(np.mean(sh)) if sh else data.get('avgOosSharpe')
        contiguous = True
        prev_end = None
        for w in windows:
            a, b = _parse_ts(w.get('testStart')), _parse_ts(w.get('testEnd'))
            if a is None or b is None:
                contiguous = False
                break
            if prev_end is not None and (a - prev_end).days > 7:
                contiguous = False
            prev_end = b
        if contiguous:
            data['compoundedOosReturn'] = float(np.prod([1.0 + r for r in oos]) - 1.0)
        else:
            data['compoundedOosReturn'] = None
            warnings.append({'code': 'OOS_NOT_CONTIGUOUS', 'severity': 'info', 'symbol': None,
                             'message': 'The test windows do not cover the history contiguously, so their returns '
                                        'cannot be compounded into one out-of-sample result.'})
        is_sh = [_f(w.get('inSampleSharpe')) for w in windows if _f(w.get('inSampleSharpe')) is not None]
        data['degradationRatio'] = (float(np.mean(sh)) / float(np.mean(is_sh))) if (
            real and sh and is_sh and np.mean(is_sh) > 0) else None
        ppy = 252.0
        is_ann, oos_ann = [], []
        for w in windows:
            tb, sb = _f(w.get('trainBars')), _f(w.get('testBars'))
            ir, orr = _f(w.get('inSampleReturn')), _f(w.get('outOfSampleReturn'))
            if real and tb and ir is not None and ir > -1:
                is_ann.append((1 + ir) ** (ppy / tb) - 1)
            if sb and orr is not None and orr > -1:
                oos_ann.append((1 + orr) ** (ppy / sb) - 1)
        data['walkForwardEfficiency'] = (float(np.mean(oos_ann)) / float(np.mean(is_ann))) if (
            is_ann and oos_ann and np.mean(is_ann) > 0) else None
    data.setdefault('mode', None)
    data.setdefault('purgeBars', None)
    data.setdefault('embargoBars', None)
    data['warnings'] = warnings
    data['engine'] = ident.engine
    data['engineImplementation'] = ident.implementation
