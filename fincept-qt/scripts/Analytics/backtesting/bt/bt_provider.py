"""
"bt" engine provider - the terminal's entry point (<command> --stdin, one JSON line on stdout).

WHAT RUNS (every result says it in `engine` / `engineImplementation` / assumptions / provenance)
  run_backtest   bt_engine.run_backtest: bars from base.price_store (session dates, mixed calendars
                 aligned with a CALENDAR_MISMATCH warning).
                   allocation strategies (equal weight, inverse vol, mean-variance, minimum
                   variance, risk parity, target vol, momentum top-N, dual momentum, buy & hold):
                   the bt LIBRARY's algo stack decides the target weights (lookbacks in bars);
                   base.simulator executes them (weights mode) at next_open / same_close, and
                   bt.Backtest itself executes them at fillTiming next_close (bt's own accounting).
                   Trades are position episodes (no FIFO fragments of rebalances).
                   signal strategies (SMA / EMA / MACD / ADX / z-score / Bollinger / RSI /
                   stochastic / Donchian / ROC momentum): base.simulator signals mode - the bt
                   library is not used for them and the identity says "base.simulator".
                 Result: base.result_normalizer.build_backtest_result.
  optimize       base.robustness.run_optimization over bt_evaluate.evaluate (bars loaded once;
                 seeded sampling, deflated Sharpe, PBO); the identity names who decided (bt algo
                 stack / signal rules) and who executed (base.simulator).
  walk_forward   base.robustness.run_walk_forward (re-optimised on every training window,
                 warm-up bars, tiling test windows, stitched out-of-sample equity).
  monte_carlo    base.monte_carlo over the run_backtest result.
  calculate_indicator / get_historical_data   bt_strategies indicators on price-store bars.

  research       indicator_signals / indicator_param_sweep / generate_signals / generate_labels /
                 labels_to_signals / generate_splits / analyze_returns: the Fincept research tools
                 shared with the vectorbt engine (vectorbt\\vbt_research.py: price-store bars, one
                 indicator id table, base.metrics, base.simulator), labelled engine "bt" with an
                 engineImplementation that says so. v1's own copies downloaded from Yahoo and returned
                 shapes the screen could not render (BacktestingTypes.h hid them for bt).

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

RESEARCH_COMMANDS = ('indicator_signals', 'indicator_param_sweep', 'generate_signals', 'generate_labels',
                     'labels_to_signals', 'generate_splits', 'analyze_returns')
COMMANDS = ('run_backtest', 'optimize', 'walk_forward', 'monte_carlo', 'get_strategies', 'get_indicators',
            'get_command_options', 'get_capabilities', 'get_historical_data', 'calculate_indicator', 'test_import',
            'test_connection', 'initialize', 'disconnect') + RESEARCH_COMMANDS
RESEARCH_IMPLEMENTATION = ('Fincept research tools shared with the vectorbt engine (vbt_research: price-store bars, '
                           'vbt_indicators, base.metrics, base.simulator) - the bt library is not used')


def _shared_research(command: str):
    """A research command served by the shared Fincept research tools (vectorbt\\vbt_research.py)."""
    def run(args):
        from base.base_provider import error_envelope
        vdir = str(_BACKTESTING_DIR / 'vectorbt')
        if vdir not in sys.path:
            sys.path.append(vdir)          # appended: this engine's own modules keep precedence
        import vbt_engine as VE
        import vbt_research as RS
        try:
            out = getattr(RS, command)(args)
        except (RS.ResearchInputError, VE.EngineInputError) as e:
            return error_envelope(str(e), 'INPUT')
        except VE.EngineDataError as e:
            return error_envelope(str(e), 'DATA')
        d = out.get('data') if isinstance(out, dict) else None
        if isinstance(d, dict):
            d['engine'] = 'bt'
            d['engineImplementation'] = RESEARCH_IMPLEMENTATION
        return out
    return run


def _identity_text() -> str:
    import bt_engine as E
    return E.short_implementation()


def _library_check(args=None):
    """None when the bt library can run this request, else a MISSING_PACKAGE envelope. Signal strategies do
    not need the library (they run on base.simulator)."""
    from base.base_provider import error_envelope
    from bt_data import library_installed
    import bt_strategies as ST
    if isinstance(args, dict):
        strat = args.get('strategy') if isinstance(args.get('strategy'), dict) else {}
        try:
            if ST.kind_of(ST.resolve(strat.get('type') or 'sma_crossover')) == 'signals':
                return None
        except ValueError:
            return None    # the engine reports the unknown strategy itself
    if not library_installed():
        return error_envelope('The bt library is not installed (pip package "bt"); allocation strategies need it. '
                              'Install it in the backtesting Python environment, or choose a signal strategy.',
                              'MISSING_PACKAGE')
    return None


def get_strategies(_args):
    """The catalogue (vectorbt array shape). Every row: supported (this engine implements it), runnable (it
    can run in THIS Python environment), reason (null unless one of them is false); counts at the top level
    (total / supported / runnable_here)."""
    import bt_strategies as ST
    from bt_data import library_installed
    rows = ST.get_strategy_catalog()
    have = library_installed()
    for r in rows:
        r['supported'] = True
        r['runnable'] = have or r['kind'] == 'signals'
        r['reason'] = None if r['runnable'] else ('needs the bt library (pip package "bt"), which is not installed '
                                                  'in the backtesting Python environment')
        if r['minSymbols'] > 1:
            r['requiresSymbols'] = r['minSymbols']
    runnable = sum(1 for r in rows if r['runnable'])
    return {'success': True, 'data': rows, 'provider': 'bt', 'total': len(rows), 'supported': len(rows),
            'runnable_here': runnable}


INDICATORS = {
    'sma': ('Simple Moving Average', 'trend', ['period']),
    'ema': ('Exponential Moving Average', 'trend', ['period']),
    'macd': ('MACD', 'trend', ['fast', 'slow', 'signal']),
    'adx': ('ADX (Wilder)', 'trend', ['period']),
    'donchian': ('Donchian Channels', 'trend', ['period']),
    'rsi': ('RSI (Wilder)', 'momentum', ['period']),
    'stoch': ('Stochastic', 'momentum', ['kPeriod', 'dPeriod']),
    'momentum': ('Rate of Change', 'momentum', ['period']),
    'bbands': ('Bollinger Bands', 'volatility', ['period', 'stdDev']),
    'zscore': ('Z-Score', 'volatility', ['period']),
    'mstd': ('Rolling Std Dev', 'volatility', ['period']),
}


def get_indicators(_args):
    grouped = {}
    for iid, (label, cat, params) in INDICATORS.items():
        grouped.setdefault(cat, []).append({'id': iid, 'name': label, 'params': list(params)})
    return {'success': True, 'indicators': grouped, 'data': {'indicators': grouped}}


def get_command_options(_args):
    from base.execution import FILL_MODES
    from base.robustness import OBJECTIVES
    import bt_engine as E
    return {'success': True, 'data': {
        'position_sizing_methods': list(E.SIZING_MODES),
        'optimize_objectives': ['sharpe', 'sortino', 'calmar', 'return'] +
                               [o for o in OBJECTIVES if o not in ('sharpe', 'sortino', 'calmar', 'return')],
        'optimize_methods': ['grid', 'random'],
        'walk_forward_modes': ['rolling', 'anchored'],
        'fill_timings': list(FILL_MODES),
        'rebalance_periods': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly'],
        'label_types': [], 'splitter_types': [], 'signal_generators': [], 'indicator_signal_modes': [],
        'returns_analysis_types': [],
    }}


REQUEST_KEYS = {
    'honoured': ['symbols', 'startDate', 'endDate', 'interval', 'initialCapital', 'commission', 'slippage',
                 'costModel', 'fillTiming', 'positionSizing', 'positionSizeValue', 'fractionalShares', 'stopLoss',
                 'takeProfit', 'trailingStop', 'allowShort', 'benchmarkSymbol', 'riskFreeRate', 'riskFreeSource',
                 'allowSyntheticData', 'brokerCandles', 'strategy', 'btExecution', 'paramRanges', 'optimizeMethod',
                 'optimizeObjective', 'maxIterations', 'seed', 'holdoutFraction', 'maxWorkers', 'wfSplits',
                 'wfTrainRatio', 'wfMode', 'purgeBars', 'embargoBars', 'mcMethod', 'mcSimulations', 'mcSeed',
                 'mcBlockSize', 'mcRuinLevel', 'cancelFile'],
    'ignored': ['leverage', 'weights'],
    'notes': {
        'strategy': 'allocation strategies: the bt library algo stack decides the target weights; signal strategies '
                    'run on base.simulator (the bt library is not used for them)',
        'fillTiming': 'next_open (default) and same_close: base.simulator executes; next_close: allocation strategies '
                      'are executed by bt.Backtest itself (bt\'s native convention: the decision of bar t traded at '
                      'bar t+1\'s close), signal strategies by base.simulator',
        'btExecution': '"auto" (default) | "simulator" (keep next_close allocation runs on base.simulator) | "bt"',
        'fractionalShares': 'default false: whole shares (bt\'s integer positions); true = fractional',
        'positionSizing': 'percent only: signal strategies take that % of each symbol\'s slot (equity / symbols) per '
                          'entry; allocation strategies invest that % of the target weights',
        'stopLoss': 'signal strategies only (intrabar on high / low); ignored for allocation strategies',
        'allowShort': 'signal strategies only: an exit signal opens a short (borrow fee from the cost model)',
        'costModel': 'pct_notional / per_share / per_trade commission with minimum and maximum; pct_price / '
                     'vol_scaled slippage + half spread on every fill price (bt\'s native path books them through '
                     'its cost hook and the engine itemises them); cash interest / financing / impact / participation '
                     'cap move a next_close allocation run to base.simulator',
        'leverage': 'positions are capped at available cash; target_vol is capped at 100 % invested',
        'weights': 'allocation weights come from the strategy (bt algo stack); request weights are not used',
        'maxWorkers': 'a cap (also FINCEPT_BT_MAX_WORKERS); one process when the work is small',
        'rebalancePeriod': 'strategy parameter of the allocation strategies: daily | weekly | monthly | quarterly | '
                           'yearly (bt Run* algo, first bar of each period) or a number of bars (RunEveryNPeriods)'}}


def get_capabilities(_args):
    from base.base_provider import NOT_MODELLED_VOCABULARY, capabilities_payload
    from base.costs import PRESETS
    from base.execution import fill_model_block
    from base.monte_carlo import METHODS as MC_METHODS
    from base.price_store import INTERVALS
    from bt_data import library_installed, library_version
    import bt_engine as E
    modelled = {'borrow cost', 'cash interest', 'volume limits'}   # base.simulator (signals / allocation off next_close)
    data = capabilities_payload(
        'bt', 'bt', library_version(), installed=library_installed(), commands=list(COMMANDS),
        fill_model=fill_model_block('next_open'), commission_models=['pct_notional', 'per_share', 'per_trade'],
        slippage_models=['pct_price', 'vol_scaled'], short_selling=True, leverage=False, multi_symbol=True,
        intraday=True, sizing_methods=list(E.SIZING_MODES), stops=True, monte_carlo_methods=list(MC_METHODS),
        optimize_methods=['grid', 'random'], walk_forward_modes=['rolling', 'anchored'],
        not_modelled=[x for x in NOT_MODELLED_VOCABULARY if x not in modelled],
        engine_implementation=_identity_text(), fill_timings=['next_open', 'next_close', 'same_close'],
        intervals=list(INTERVALS), cost_presets=sorted(PRESETS), request_keys=REQUEST_KEYS)
    data.update({'shares': 'whole', 'fractionalShares': True, 'maxSymbolsPerRun': None, 'weightsBlock': True,
                 'stopTypes': ['stopLoss', 'takeProfit', 'trailingStop'], 'stopsApplyTo': 'signal strategies',
                 'shortSellingAppliesTo': 'signal strategies', 'costSensitivity': True,
                 'executors': {'allocation': {'next_open': 'base.simulator', 'same_close': 'base.simulator',
                                              'next_close': 'bt.Backtest'},
                               'signals': {'next_open': 'base.simulator', 'same_close': 'base.simulator',
                                           'next_close': 'base.simulator'}},
                 'tradeReporting': {'signals': 'round trips', 'allocation': 'position episodes'},
                 'rebalancePeriods': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']})
    return {'success': True, 'data': data}


def get_historical_data(args):
    from base.metrics import _fmt_date
    from base.price_store import get_bars, normalize_symbol
    syms = args.get('symbols') or []
    if isinstance(syms, str):
        syms = [s.strip() for s in syms.split(',') if s.strip()]
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
    """Any indicator get_indicators lists, on price-store bars: {indicator, symbol, totalBars, series, stats}.
    Unknown id = INPUT error."""
    import numpy as np
    import pandas as pd
    import bt_engine as E
    import bt_strategies as ST
    from base.metrics import _fmt_date
    from base.price_store import get_bars, normalize_symbol
    ind = str(args.get('indicator') or args.get('type') or '').strip().lower()
    ind = {'ma': 'sma', 'stochastic': 'stoch', 'roc': 'momentum', 'bollinger': 'bbands'}.get(ind, ind)
    if ind not in INDICATORS:
        raise E.EngineInputError(f'Indicator {ind!r} is not offered by the bt engine (choose one of '
                                 f'{", ".join(INDICATORS)}).')
    sym = args.get('symbol') or ((args.get('symbols') or ['SPY'])[0])
    sym = normalize_symbol(sym)
    p = dict(args.get('params') or args.get('parameters') or {}) if isinstance(
        args.get('params') or args.get('parameters') or {}, dict) else {}

    def gi(key, default):
        return int(float(p.get(key, args.get(key, default)) or default))
    res = get_bars([sym], str(args.get('startDate') or '2020-01-01')[:10], str(args.get('endDate') or '')[:10]
                   or pd.Timestamp.today().strftime('%Y-%m-%d'), interval=str(args.get('interval') or '1d'),
                   progress=False)
    f = res.frames.get(sym)
    if f is None or len(f) == 0:
        raise E.EngineDataError(f'No data for {sym}: {res.errors.get(sym) or "no bars in the period"}.')
    o, h, lo, c = (f[x].to_numpy(float) for x in ('Open', 'High', 'Low', 'Close'))
    if ind == 'sma':
        out = ST.sma(c, gi('period', 20))
    elif ind == 'ema':
        from base.signals import ema
        out = ema(c, gi('period', 20))
    elif ind == 'macd':
        out = ST.macd(c, gi('fast', 12), gi('slow', 26), gi('signal', 9))
    elif ind == 'adx':
        out = ST.adx(h, lo, c, gi('period', 14))
    elif ind == 'donchian':
        n = gi('period', 20)
        out = {'upper': ST.rolling_max(h, n), 'lower': ST.rolling_min(lo, n)}
    elif ind == 'rsi':
        out = ST.rsi(c, gi('period', 14))
    elif ind == 'stoch':
        out = ST.stochastic(h, lo, c, gi('kPeriod', 14), gi('dPeriod', 3))
    elif ind == 'momentum':
        out = ST.roc(c, gi('period', 12))
    elif ind == 'bbands':
        n = gi('period', 20)
        k = float(p.get('stdDev', args.get('stdDev', 2.0)) or 2.0)
        m, sd = ST.sma(c, n), ST.rolling_std(c, n, ddof=0)
        out = {'upper': m + k * sd, 'middle': m, 'lower': m - k * sd}
    elif ind == 'zscore':
        n = gi('period', 20)
        m, sd = ST.sma(c, n), ST.rolling_std(c, n, ddof=1)
        with np.errstate(divide='ignore', invalid='ignore'):
            out = (c - m) / sd
    else:
        out = ST.rolling_std(c, gi('period', 20), ddof=1)
    dates = [_fmt_date(t) for t in f.index]

    def series(values):
        arr = np.asarray(values, dtype=float)
        return [{'date': d, 'value': (float(v) if np.isfinite(v) else None)} for d, v in zip(dates, arr)]

    def stats(values):
        arr = np.asarray(values, dtype=float)
        arr = arr[np.isfinite(arr)]
        if not len(arr):
            return None
        return {'last': float(arr[-1]), 'mean': float(arr.mean()), 'std': float(arr.std()), 'min': float(arr.min()),
                'max': float(arr.max())}
    data = {'indicator': ind, 'symbol': sym, 'totalBars': int(len(f)), 'usingSyntheticData': False, 'engine': 'bt',
            'engineImplementation': 'Fincept indicators (bt_strategies, numpy / pandas) on price-store bars'}
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
    import bt_engine as E

    def bt(req):
        try:
            return E.run_backtest(req)
        except Exception as e:  # surfaces as the Monte Carlo failure envelope
            return _error_from(e)
    return run_monte_carlo(bt, args)


def _error_from(e: BaseException):
    import traceback
    from base.base_provider import error_envelope
    import bt_engine as E
    try:
        from base.price_store import PriceDataError
    except Exception:  # pragma: no cover
        PriceDataError = RuntimeError
    if isinstance(e, E.EngineInputError):
        return error_envelope(str(e), 'INPUT')
    if isinstance(e, (E.EngineDataError, PriceDataError)):
        return error_envelope(str(e), 'DATA')
    if isinstance(e, ImportError):
        return error_envelope(f'A package the bt engine needs is missing: {e}.', 'MISSING_PACKAGE')
    if isinstance(e, (ValueError, KeyError, TypeError)):
        return error_envelope(str(e) or type(e).__name__, 'INPUT', traceback_text=traceback.format_exc())
    return error_envelope(f'{type(e).__name__}: {e}', 'ENGINE', traceback_text=traceback.format_exc())


_NEEDS_LIBRARY = ('run_backtest', 'optimize', 'walk_forward', 'monte_carlo')


def dispatch(command: str, args: dict) -> dict:
    import bt_engine as E
    from bt_data import library_installed, library_version
    args = args if isinstance(args, dict) else {}
    if command in _NEEDS_LIBRARY:
        bad = _library_check(args)
        if bad is not None:
            return bad
    simple = {'test_import': lambda a: {'success': True, 'version': E.ENGINE_VERSION, 'engine': 'bt', 'library': 'bt',
                                        'libraryVersion': library_version(), 'installed': library_installed(),
                                        'engineImplementation': _identity_text()},
              'test_connection': lambda a: {'success': True, 'message': f'{_identity_text()} is available'
                                            if library_installed() else 'bt library not installed: signal strategies '
                                                                        'run on base.simulator, allocation strategies '
                                                                        'need the bt package'},
              'initialize': lambda a: {'success': True, 'message': f'{_identity_text()} ready'},
              'disconnect': lambda a: {'success': True, 'message': 'Disconnected'}}
    table = {'run_backtest': E.run_backtest, 'optimize': E.optimize, 'walk_forward': E.walk_forward,
             'monte_carlo': _monte_carlo, 'get_strategies': get_strategies, 'get_indicators': get_indicators,
             'get_command_options': get_command_options, 'get_capabilities': get_capabilities,
             'get_historical_data': get_historical_data, 'calculate_indicator': calculate_indicator, **simple,
             **{c: _shared_research(c) for c in RESEARCH_COMMANDS}}
    fn = table.get(command)
    if fn is None:
        from base.base_provider import error_envelope
        return error_envelope(f'Unknown command: {command}', 'INPUT')
    try:
        return fn(args)
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
