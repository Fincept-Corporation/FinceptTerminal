"""
"Backtesting.py" engine provider - the terminal's entry point (<command> --stdin, one JSON line on
stdout).

WHAT RUNS (every result says it in `engine` / `engineImplementation` / assumptions / provenance)
  run_backtest   btp_engine.run_backtest: bars from base.price_store, the backtesting.py LIBRARY runs
                 the strategy (its event loop, broker, order matching, whole-share sizing, contingent
                 stop orders; btp_executor adds slippage on every fill, borrow fees, fill-price stop
                 levels and a fill log), result from base.result_normalizer.build_backtest_result
                 (base.metrics headline figures; the library's own summary under engineReported).
  optimize       base.robustness.run_optimization over btp_evaluate.evaluate - every combination is a
                 backtesting.py run with the same settings as run_backtest (seeded sampling, exact
                 float ranges, deflated Sharpe, PBO).
  walk_forward   base.robustness.run_walk_forward (re-optimised on every training window, warm-up bars,
                 tiling test windows, stitched out-of-sample equity).
  monte_carlo    base.monte_carlo over the run_backtest result.
  calculate_indicator / get_historical_data   btp_indicators on price-store bars.

Progress lines go to STDERR (@@BT_PROGRESS, base.progress); the host's cancelFile stops long loops
(exit code 3 + the cancelled envelope).
"""

import json
import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
_BACKTESTING_DIR = _SCRIPT_DIR.parent
for _p in (str(_BACKTESTING_DIR), str(_SCRIPT_DIR)):
    if _p not in sys.path:
        sys.path.insert(0, _p)

COMMANDS = ('run_backtest', 'optimize', 'walk_forward', 'monte_carlo', 'get_strategies', 'get_indicators',
            'get_command_options', 'get_capabilities', 'get_historical_data', 'calculate_indicator', 'test_import',
            'test_connection', 'initialize', 'disconnect')


def _identity_text() -> str:
    import btp_engine as E
    return E.implementation()


def _library_check():
    """None when backtesting.py is importable with the hooks the engine uses, else an error envelope."""
    from base.base_provider import error_envelope
    try:
        import btp_executor as X
    except ImportError as e:
        return error_envelope(f'The backtesting.py library is not installed ({e}). Install the "backtesting" package '
                              f'in the backtesting Python environment.', 'MISSING_PACKAGE')
    why = X.library_ok()
    if why:
        return error_envelope(f'Unsupported backtesting.py version: {why}.', 'MISSING_PACKAGE')
    return None


def _library_installed() -> bool:
    import importlib.util
    try:
        return importlib.util.find_spec('backtesting') is not None
    except (ImportError, ValueError):
        return False


def get_strategies(_args):
    """The catalogue (vectorbt array shape). Every row: supported (this engine implements it), runnable
    (it can run in THIS Python environment) and reason (null unless one of them is false); counts at
    the top level like the fincept provider (total / supported / runnable_here)."""
    import btp_strategies as ST
    rows = ST.get_strategy_catalog()
    have = _library_installed()
    for r in rows:
        r['supported'] = True
        r['runnable'] = have
        r['reason'] = None if have else 'needs the backtesting package (backtesting.py), which is not installed'
    return {'success': True, 'data': rows, 'provider': 'backtestingpy', 'total': len(rows),
            'supported': len(rows), 'runnable_here': len(rows) if have else 0}


def get_indicators(_args):
    import btp_indicators as I
    grouped = {}
    for entry in I.get_catalog():
        grouped.setdefault(entry.get('category', 'Other'), []).append(
            {'id': entry['id'], 'name': entry.get('label', entry['id']), 'params': list(entry.get('params') or [])})
    return {'success': True, 'indicators': grouped, 'data': {'indicators': grouped}}


def get_command_options(_args):
    from base.execution import FILL_MODES
    from base.robustness import OBJECTIVES
    import btp_engine as E
    return {'success': True, 'data': {
        'position_sizing_methods': list(E.SIZING_MODES),
        'optimize_objectives': ['sharpe', 'sortino', 'calmar', 'return'] +
                               [o for o in OBJECTIVES if o not in ('sharpe', 'sortino', 'calmar', 'return')],
        'optimize_methods': ['grid', 'random'],
        'walk_forward_modes': ['rolling', 'anchored'],
        'fill_timings': list(FILL_MODES),
        'label_types': [], 'splitter_types': [], 'signal_generators': [], 'indicator_signal_modes': [],
        'returns_analysis_types': [],
    }}


REQUEST_KEYS = {
    'honoured': ['symbols', 'startDate', 'endDate', 'interval', 'initialCapital', 'commission', 'slippage',
                 'costModel', 'fillTiming', 'leverage', 'allowShort', 'positionSizing', 'positionSizeValue',
                 'stopLoss', 'takeProfit', 'trailingStop', 'benchmarkSymbol', 'riskFreeRate', 'riskFreeSource',
                 'allowSyntheticData', 'brokerCandles', 'strategy', 'paramRanges', 'optimizeMethod',
                 'optimizeObjective', 'maxIterations', 'seed', 'holdoutFraction', 'maxWorkers', 'wfSplits',
                 'wfTrainRatio', 'wfMode', 'purgeBars', 'embargoBars', 'mcMethod', 'mcSimulations', 'mcSeed',
                 'mcBlockSize', 'mcRuinLevel', 'cancelFile'],
    'ignored': ['weights', 'fractionalShares', 'cashRateAnnual', 'financingRateAnnual'],
    'notes': {
        'symbols': 'backtesting.py tests ONE instrument per run: the first symbol is traded, the others are listed '
                   'under ignoredSettings with IGNORED_SETTING / IGNORED_SYMBOLS warnings',
        'costModel': 'pct_notional / per_share / per_trade commission (with minimum / maximum) through the '
                     "library's commission (a callable when needed); pct_price / vol_scaled slippage and half the "
                     "spread on every fill price; borrow on shorts. impactCoef and participationCap are not applied "
                     '(ignoredSettings + IGNORED_SETTING warning)',
        'fillTiming': 'next_open = library market orders; same_close = library trade_on_close; next_close = decision '
                      'placed one bar later with trade_on_close (refused for custom strategy code)',
        'leverage': 'backtesting.py margin = 1 / leverage (liquidation at equity 0; no financing cost)',
        'allowShort': 'exit signals open shorts (library sell); borrow fee from the cost model',
        'positionSizing': 'percent / fixed / risk / vol_target / kelly -> order size as a fraction of available '
                          'margin; whole units',
        'maxWorkers': 'a cap (also FINCEPT_BT_MAX_WORKERS); one process when the work is small',
        'weights': 'single instrument per run - no weights block',
        'fractionalShares': 'backtesting.py trades whole units'}}


def get_capabilities(_args):
    from base.base_provider import capabilities_payload
    from base.execution import fill_model_block
    from base.costs import PRESETS
    from base.monte_carlo import METHODS as MC_METHODS
    from base.price_store import INTERVALS
    import btp_engine as E
    data = capabilities_payload(
        'backtestingpy', 'backtesting', E.lib_version(), installed=_library_check() is None, commands=list(COMMANDS),
        fill_model=fill_model_block('next_open'), commission_models=['pct_notional', 'per_share', 'per_trade'],
        slippage_models=['pct_price', 'vol_scaled'], short_selling=True, leverage=True, multi_symbol=False,
        intraday=True, sizing_methods=list(E.SIZING_MODES), stops=True, monte_carlo_methods=list(MC_METHODS),
        optimize_methods=['grid', 'random'], walk_forward_modes=['rolling', 'anchored'],
        not_modelled=['bid/ask spread', 'market impact', 'partial fills', 'volume limits', 'margin calls',
                      'cash interest', 'dividend withholding', 'FX conversion', 'trading halts', 'delisted symbols'],
        engine_implementation=_identity_text(), fill_timings=['next_open', 'next_close', 'same_close'],
        intervals=list(INTERVALS), cost_presets=sorted(PRESETS), request_keys=REQUEST_KEYS)
    data.update({'shares': 'whole', 'maxSymbolsPerRun': 1, 'weightsBlock': False,
                 'stopTypes': ['stopLoss', 'takeProfit', 'trailingStop'], 'costSensitivity': True,
                 'customCode': 'a backtesting.py Strategy subclass (strategy.type "code", strategy.code)'})
    return {'success': True, 'data': data}


def get_historical_data(args):
    from base.metrics import _fmt_date
    from base.price_store import get_bars, normalize_symbol
    syms = args.get('symbols') or []
    res = get_bars(syms, str(args.get('startDate'))[:10], str(args.get('endDate'))[:10],
                   interval=str(args.get('interval') or '1d'), progress=False)
    out = []
    for s in syms:
        f = res.frames.get(normalize_symbol(s))
        rows = [] if f is None else [
            {'date': _fmt_date(t), 'open': float(r[0]), 'high': float(r[1]), 'low': float(r[2]), 'close': float(r[3]),
             'volume': float(r[4])}
            for t, r in zip(f.index, f[['Open', 'High', 'Low', 'Close', 'Volume']].to_numpy(float))]
        item = {'symbol': s, 'timeframe': args.get('timeframe', args.get('interval', 'daily')), 'data': rows}
        err = res.errors.get(normalize_symbol(s))
        if err is not None:
            item['error'] = str(err)
        out.append(item)
    return {'success': True, 'data': out}


def calculate_indicator(args):
    """Any indicator get_indicators lists (btp_indicators.INDICATOR_CATALOG; unknown id = INPUT error)
    on price-store bars: {indicator, symbol, period?, totalBars, series, stats}."""
    import numpy as np
    import pandas as pd
    import btp_engine as E
    import btp_indicators as I
    from base.metrics import _fmt_date
    from base.price_store import get_bars, normalize_symbol
    ind = str(args.get('indicator') or args.get('type') or '').strip().lower()
    ind = {'simple_moving_average': 'sma', 'exponential_moving_average': 'ema'}.get(ind, ind)
    cat = I.INDICATOR_CATALOG.get(ind)
    if cat is None:
        raise E.EngineInputError(f'Indicator {ind!r} is not offered by the Backtesting.py engine (choose one of '
                                 f'{", ".join(I.INDICATOR_CATALOG)}).')
    sym = args.get('symbol') or ((args.get('symbols') or ['SPY'])[0])
    sym = normalize_symbol(sym)
    nested = args.get('params') if isinstance(args.get('params'), dict) else {}
    period = int(float(nested.get('period', args.get('period', 20)) or 20))
    ind_params = {k: v for k, v in nested.items() if k != 'period'}
    used = None
    names = cat.get('params', [])
    if 'period' in names:
        ind_params['period'] = period
        used = period
    elif names and names[0] in ('ema_period', 'k_period', 'rsi_period'):
        ind_params[names[0]] = period
        used = period
    res = get_bars([sym], str(args.get('startDate') or '2020-01-01')[:10], str(args.get('endDate') or '')[:10]
                   or pd.Timestamp.today().strftime('%Y-%m-%d'), interval=str(args.get('interval') or '1d'),
                   progress=False)
    f = res.frames.get(sym)
    if f is None or len(f) == 0:
        raise E.EngineDataError(f'No data for {sym}: {res.errors.get(sym) or "no bars in the period"}.')
    out = I.compute(ind, f, ind_params)
    dates = [_fmt_date(t) for t in f.index]

    def series(values):
        arr = pd.Series(values).reindex(f.index).to_numpy(dtype=float) if isinstance(values, pd.Series) else \
            np.asarray(values, dtype=float)
        return [{'date': d, 'value': (float(v) if np.isfinite(v) else None)} for d, v in zip(dates, arr)]

    def stats(values):
        arr = np.asarray(pd.Series(values).to_numpy(dtype=float))
        arr = arr[np.isfinite(arr)]
        if not len(arr):
            return None
        return {'last': float(arr[-1]), 'mean': float(arr.mean()), 'std': float(arr.std()), 'min': float(arr.min()),
                'max': float(arr.max())}
    data = {'indicator': ind, 'symbol': sym, 'totalBars': int(len(f)), 'usingSyntheticData': False,
            'engine': 'backtestingpy', 'engineImplementation': 'Fincept indicator library (btp_indicators, pandas) on '
                                                               'price-store bars'}
    if used is not None:
        data['period'] = used
    if isinstance(out, dict):
        data['series'] = {k: series(v) for k, v in out.items()}
        data['stats'] = {k: s for k, s in ((k, stats(v)) for k, v in out.items()) if s is not None}
    else:
        data['series'] = series(out)
        s = stats(out)
        if s is not None:
            data['stats'] = s
    return {'success': True, 'data': data}


def _monte_carlo(args):
    from base.monte_carlo import run_monte_carlo
    import btp_engine as E

    def bt(req):
        try:
            return E.run_backtest(req)
        except Exception as e:  # surfaces as the Monte Carlo failure envelope
            return _error_from(e)
    return run_monte_carlo(bt, args)


def _error_from(e: BaseException):
    import traceback
    from base.base_provider import error_envelope
    import btp_engine as E
    try:
        from base.price_store import PriceDataError
    except Exception:  # pragma: no cover
        PriceDataError = RuntimeError
    if isinstance(e, E.EngineInputError):
        return error_envelope(str(e), 'INPUT')
    if isinstance(e, (E.EngineDataError, PriceDataError)):
        return error_envelope(str(e), 'DATA')
    if isinstance(e, (ValueError, KeyError, TypeError)):
        return error_envelope(str(e) or type(e).__name__, 'INPUT', traceback_text=traceback.format_exc())
    return error_envelope(f'{type(e).__name__}: {e}', 'ENGINE', traceback_text=traceback.format_exc())


_NEEDS_LIBRARY = ('run_backtest', 'optimize', 'walk_forward', 'monte_carlo', 'initialize', 'test_import',
                  'test_connection')


def dispatch(command: str, args: dict) -> dict:
    import btp_engine as E
    if command in _NEEDS_LIBRARY:
        bad = _library_check()
        if bad is not None:
            return bad
    simple = {'test_import': lambda a: {'success': True, 'version': E.ENGINE_VERSION, 'engine': 'backtestingpy',
                                        'library': 'backtesting', 'libraryVersion': E.lib_version(),
                                        'engineImplementation': _identity_text()},
              'test_connection': lambda a: {'success': True, 'message': f'{_identity_text()} is available'},
              'initialize': lambda a: {'success': True, 'message': f'{_identity_text()} ready'},
              'disconnect': lambda a: {'success': True, 'message': 'Disconnected'}}
    table = {'run_backtest': E.run_backtest, 'optimize': E.optimize, 'walk_forward': E.walk_forward,
             'monte_carlo': _monte_carlo, 'get_strategies': get_strategies, 'get_indicators': get_indicators,
             'get_command_options': get_command_options, 'get_capabilities': get_capabilities,
             'get_historical_data': get_historical_data, 'calculate_indicator': calculate_indicator, **simple}
    fn = table.get(command)
    if fn is None:
        from base.base_provider import error_envelope
        return error_envelope(f'Unknown command: {command}', 'INPUT')
    try:
        return fn(args if isinstance(args, dict) else {})
    except SystemExit:
        raise
    except Exception as e:
        return _error_from(e)


def main(argv=None):
    import os
    import warnings
    warnings.filterwarnings('ignore')
    os.environ.setdefault('PYTHONWARNINGS', 'ignore')
    from base.base_provider import emit_response, error_envelope, read_cli_args
    try:
        command, raw = read_cli_args(argv)
        args = json.loads(raw) if raw and raw.strip() else {}
    except (ValueError, json.JSONDecodeError) as e:
        emit_response(error_envelope(f'Bad request: {e}', 'INPUT'))
        return
    real = sys.stdout
    sys.stdout = sys.stderr          # stray prints from any library never reach the JSON line
    try:
        result = dispatch(command, args)
    finally:
        sys.stdout = real
    emit_response(result)


if __name__ == '__main__':
    main()
