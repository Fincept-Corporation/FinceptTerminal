#!/usr/bin/env python3
"""
"fincept" engine provider - the terminal's entry point (<command> --stdin, one JSON line on stdout).

Runs the strategies of the Fincept strategy registry (scripts/strategies, 418 entries of which the
catalogue marks the runnable ones) on Fincept's own QCAlgorithm-compatible bar-loop engine
(scripts/strategies/fincept_engine, driven by base/fincept_strategy_runner.py).

WHAT RUNS (every result says it in `engine` / `engineImplementation` / assumptions / provenance)
  run_backtest   fe_engine.run_backtest: the strategy's own code bar by bar in fincept_engine, whose
                 broker fills the orders (native fill: market orders at the next bar's open; fillTiming
                 next_close / same_close honoured for market orders); bars from base.price_store through
                 fincept_engine.price_feed; result from base.result_normalizer.build_backtest_result with
                 EngineIdentity('fincept', ...). base.simulator is NOT used.
  optimize       base.robustness.run_optimization over fincept_evaluate.evaluate (a full engine run per
                 combination with get_parameter values set; strategies that read no parameters: INPUT error).
  walk_forward   base.robustness.run_walk_forward (re-optimised on every training window).
  monte_carlo    base.monte_carlo over run_backtest.
  get_strategies (supported / runnable / reason / optimizable per row, counts), get_capabilities,
  get_command_options, get_indicators, get_historical_data (price store), test_import / test_connection.

Progress lines go to STDERR (@@BT_PROGRESS, base.progress): data / run / metrics / optimize /
walk_forward / monte_carlo. The host's cancelFile stops the run inside the bar loop and between trials
(exit code 3 + the cancelled envelope). A watchdog stops a strategy that hangs in initialize() (90 s) or
runs past the total limit (15 min run_backtest, 45 min the others) with a TIMEOUT envelope.
"""

import json
import os
import sys
import threading as _threading
from pathlib import Path
from typing import Any, Dict, Optional

_SCRIPT_DIR = Path(__file__).resolve().parent
_BACKTESTING_DIR = _SCRIPT_DIR.parent
for _p in (str(_BACKTESTING_DIR), str(_BACKTESTING_DIR / 'base'), str(_SCRIPT_DIR)):
    if _p not in sys.path:
        sys.path.insert(0, _p)

COMMANDS = ('run_backtest', 'execute_fincept_strategy', 'optimize', 'walk_forward', 'monte_carlo', 'get_strategies',
            'get_capabilities', 'get_command_options', 'get_indicators', 'get_historical_data', 'test_import',
            'test_connection', 'initialize')
_LONG = ('run_backtest', 'execute_fincept_strategy', 'monte_carlo', 'optimize', 'walk_forward')

# Watchdog state (see _start_watchdog): one JSON line on stdout, whoever prints first.
_OUT_LOCK = _threading.Lock()
_OUT_STATE = {'printed': False}
_DONE = _threading.Event()
_PHASE: Dict[str, Any] = {'name': 'start'}


def _num(v, default=0.0) -> float:
    try:
        f = float(v)
        return f if f == f and f not in (float('inf'), float('-inf')) else default
    except (TypeError, ValueError):
        return default


# ════════════════════════════════════════════════════════════════════════════
# Commands
# ════════════════════════════════════════════════════════════════════════════

def get_strategies(args):
    import fe_engine as E
    import fe_catalog as CAT
    include_all = bool(args.get('includeUnsupported') or args.get('include_unsupported'))
    return {'success': True, 'data': CAT.catalog(E.runner(), include_all=include_all)}


def get_capabilities(_args):
    import fe_engine as E
    from base.base_provider import capabilities_payload, NOT_MODELLED_VOCABULARY
    from base.execution import fill_model_block
    from base.costs import PRESETS
    modelled = {'margin calls', 'borrow cost'}
    data = capabilities_payload(
        'fincept', None, None, installed=True, commands=list(COMMANDS), fill_model=fill_model_block('next_open'),
        commission_models=['pct_notional', 'per_share', 'per_trade'], slippage_models=['pct_price', 'vol_scaled'],
        # stops: the request's stopLoss / takeProfit are not applied (the strategy's own stop / limit / trailing
        # orders are simulated against each bar's range - see notes); shortSelling / leverage: the engine models
        # them for what the strategy orders, the request's allowShort / leverage are ignored (requestKeys)
        short_selling=True, leverage=True, multi_symbol=True, intraday=False,
        sizing_methods=['strategy'], stops=False, optimize_methods=['grid', 'random'],
        walk_forward_modes=['rolling', 'anchored'],
        not_modelled=[x for x in NOT_MODELLED_VOCABULARY if x not in modelled],
        engine_implementation=E.IMPLEMENTATION, fill_timings=['next_open', 'next_close', 'same_close'],
        intervals=['1d'], cost_presets=sorted(PRESETS),
        request_keys={
            'honoured': ['symbols', 'startDate', 'endDate', 'initialCapital', 'commission', 'slippage', 'costModel',
                         'fillTiming', 'benchmarkSymbol', 'riskFreeRate', 'riskFreeSource', 'strategy', 'paramRanges',
                         'optimizeMethod', 'optimizeObjective', 'maxIterations', 'seed', 'holdoutFraction',
                         'maxWorkers', 'wfSplits', 'wfTrainRatio', 'wfMode', 'purgeBars', 'embargoBars', 'mcMethod',
                         'mcSimulations', 'mcSeed', 'mcBlockSize', 'mcRuinLevel', 'cancelFile', 'includeUnsupported',
                         'allowSyntheticData'],
            'ignored': ['positionSizing', 'positionSizeValue', 'stopLoss', 'takeProfit', 'trailingStop', 'leverage',
                        'allowShort', 'interval'],
            'notes': {
                'positionSizing': 'the strategy code sizes its own orders (set_holdings / order quantities)',
                'positionSizeValue': 'the strategy code sizes its own orders',
                'stopLoss': 'the strategy code places its own exits (its stop / limit / trailing orders are '
                            'simulated against each bar\'s range)',
                'takeProfit': 'the strategy code places its own exits',
                'trailingStop': 'the strategy code places its own exits',
                'leverage': 'each security uses the strategy\'s own leverage, capped per asset class (equities 2x, '
                            'forex 50x, crypto / futures / indices 1x by default); margin calls are modelled',
                'allowShort': 'the strategy code decides whether to sell short; borrow is charged from the cost '
                              'model (allowShort false with a strategy that shorts: IGNORED_SETTING)',
                'interval': 'the engine steps through daily bars only (intraday subscriptions run once a day)',
                'costModel': 'commission pct / per share / per trade with min / max, slippage pct / vol-scaled + half '
                             'spread, borrow are applied by the engine\'s broker; impactCoef, participationCap, '
                             'cashRateAnnual, financingRateAnnual are not (IGNORED_SETTING when set)',
                'fillTiming': 'next_open is the engine\'s native rule; next_close / same_close move MARKET orders '
                              'only (limit / stop / MOO / MOC keep their own rules)',
                'paramRanges': 'only parameters the strategy reads with get_parameter (catalogue row optimizable)',
                'maxWorkers': 'capped by FINCEPT_BT_MAX_WORKERS; one process when the work is small',
                'allowSyntheticData': 'the engine never runs on synthetic prices',
                'costs.sensitivity': E.SENSITIVITY_NOTE}})
    data['strategyApi'] = 'QCAlgorithm (LEAN-compatible Python API)'
    data['executor'] = 'fincept_engine broker (base.simulator is not used)'
    data['catalogue'] = 'get_strategies rows carry supported / runnable / reason / optimizable'
    return {'success': True, 'data': data}


def get_command_options(_args):
    from base.robustness import OBJECTIVES
    return {'success': True, 'data': {
        'position_sizing_methods': ['strategy'],
        'optimize_objectives': ['sharpe', 'sortino', 'calmar', 'return'] +
                               [o for o in OBJECTIVES if o not in ('sharpe', 'sortino', 'calmar', 'return')],
        'optimize_methods': ['grid', 'random'],
        'walk_forward_modes': ['rolling', 'anchored'],
        'fill_timings': ['next_open', 'next_close', 'same_close'],
        'label_types': [], 'splitter_types': [], 'signal_generators': [], 'indicator_signal_modes': [],
        'returns_analysis_types': [],
    }}


def get_indicators(_args):
    """Fincept strategies are self-contained - no separate indicator catalogue."""
    return {'success': True, 'indicators': {}, 'data': {'indicators': {}}}


def get_historical_data(args):
    from base.price_store import get_bars, normalize_symbol
    from base.metrics import _fmt_date
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


def _run_backtest(args):
    import fe_engine as E
    E.runner().phase = _PHASE
    return E.run_backtest(args)


def _monte_carlo(args):
    from base.monte_carlo import run_monte_carlo

    def bt(req):
        try:
            return _run_backtest(req)
        except Exception as e:  # surfaces as the Monte Carlo failure envelope
            return _error_from(e)
    return run_monte_carlo(bt, args)


def _optimize(args):
    import fe_engine as E
    E.runner().phase = _PHASE
    return E.optimize(args)


def _walk_forward(args):
    import fe_engine as E
    E.runner().phase = _PHASE
    return E.walk_forward(args)


def _error_from(e: BaseException):
    import traceback
    from base.base_provider import error_envelope
    import fe_engine as E
    try:
        from base.price_store import PriceDataError
    except Exception:  # pragma: no cover
        PriceDataError = RuntimeError
    if isinstance(e, E.EngineInputError):
        return error_envelope(str(e), 'INPUT')
    if isinstance(e, (E.EngineDataError, PriceDataError)):
        return error_envelope(str(e), 'DATA')
    if isinstance(e, E.EngineRunError):
        env = error_envelope(str(e), e.code, traceback_text=e.traceback_text)
        if e.not_supported:
            env['notSupported'] = True
            env['feature'] = e.feature
        return env
    if isinstance(e, (ValueError, KeyError, TypeError)):
        return error_envelope(str(e) or type(e).__name__, 'INPUT', traceback_text=traceback.format_exc())
    return error_envelope(f'{type(e).__name__}: {e}', 'ENGINE', traceback_text=traceback.format_exc())


def dispatch(command: str, args: dict) -> dict:
    import fe_engine as E
    simple = {'test_import': lambda a: {'success': True, 'version': E.ENGINE_VERSION, 'engine': 'fincept',
                                        'library': None, 'engineImplementation': E.IMPLEMENTATION},
              'test_connection': lambda a: {'success': True, 'message': f'{E.SHORT_IMPLEMENTATION} is available'},
              'initialize': lambda a: {'success': True, 'message': f'{E.SHORT_IMPLEMENTATION} ready'}}
    table = {'run_backtest': _run_backtest, 'execute_fincept_strategy': _run_backtest, 'optimize': _optimize,
             'walk_forward': _walk_forward, 'monte_carlo': _monte_carlo, 'get_strategies': get_strategies,
             'get_capabilities': get_capabilities, 'get_command_options': get_command_options,
             'get_indicators': get_indicators, 'get_historical_data': get_historical_data, **simple}
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


# ════════════════════════════════════════════════════════════════════════════
# Watchdog and output
# ════════════════════════════════════════════════════════════════════════════

def _limit(env_name: str, default: float, request: Optional[Dict[str, Any]] = None, key: str = '') -> float:
    val = None
    if request and key and request.get(key) not in (None, ''):
        val = _num(request.get(key), 0.0)
    if not val:
        val = _num(os.environ.get(env_name), 0.0)
    return val if val and val > 0 else default


def _start_watchdog(command: str, request: Dict[str, Any]) -> None:
    """Wall-clock guard. A strategy that hangs (a sleeping train() in initialize(), a loop over a huge
    dataset) used to hold one of the host's runner slots until its 60-min kill. Past the limit this
    thread prints the TIMEOUT envelope, flushes stdout and hard-exits (exit code 0, like every other
    error envelope). A lock guarantees exactly one JSON line even if the run finishes at the same
    moment. Threads + os._exit work on Windows too (no signal.alarm)."""
    import time
    init_limit = _limit('FINCEPT_BT_INIT_TIMEOUT', 90.0, request, 'initTimeoutSeconds')
    total_default = 900.0 if command in ('run_backtest', 'execute_fincept_strategy') else 2700.0
    total_limit = _limit('FINCEPT_BT_TIMEOUT', total_default, request, 'timeoutSeconds')
    t0 = time.monotonic()

    def _watch():
        while not _DONE.wait(0.25):
            now = time.monotonic()
            msg = None
            if _PHASE.get('name') == 'initialize' and now - _PHASE.get('since', now) > init_limit:
                msg = (f"The strategy's initialize() did not finish within {init_limit:.0f} s and was stopped "
                       f"(it may wait on a long training step or loop over data the engine does not have).")
            elif now - t0 > total_limit:
                msg = (f"The backtest did not finish within {total_limit / 60:.0f} min and was stopped "
                       f"(phase: {_PHASE.get('name', '?')}).")
            if msg:
                from base.base_provider import error_envelope, json_response
                with _OUT_LOCK:
                    if _OUT_STATE['printed']:
                        return
                    _OUT_STATE['printed'] = True
                    try:
                        env = error_envelope(msg, 'TIMEOUT', timedOut=True)
                        sys.__stdout__.write(json_response(env) + '\n')
                        sys.__stdout__.flush()
                        sys.stderr.flush()
                    finally:
                        os._exit(0)

    _threading.Thread(target=_watch, name='fincept-bt-watchdog', daemon=True).start()


def _emit(result) -> None:
    """Print the single JSON line unless the watchdog already did."""
    from base.base_provider import json_response
    text = json_response(result)
    with _OUT_LOCK:
        if _OUT_STATE['printed']:
            return
        _OUT_STATE['printed'] = True
        out = sys.__stdout__
        try:
            out.reconfigure(encoding='utf-8', errors='replace')
        except Exception:
            pass
        out.write(text + '\n')
        out.flush()
    _DONE.set()


def main(argv=None):
    import warnings
    warnings.filterwarnings('ignore')
    os.environ.setdefault('PYTHONWARNINGS', 'ignore')
    from base.base_provider import error_envelope, read_cli_args
    try:
        command, raw = read_cli_args(argv)
        args = json.loads(raw) if raw and raw.strip() else {}
    except (ValueError, json.JSONDecodeError) as e:
        _emit(error_envelope(f'Bad request: {e}', 'INPUT'))
        return
    if command in _LONG:
        _start_watchdog(command, args if isinstance(args, dict) else {})
    real = sys.stdout
    sys.stdout = sys.stderr          # strategy prints / library chatter never reach the JSON line
    try:
        result = dispatch(command, args)
    finally:
        sys.stdout = real
    _emit(result)


if __name__ == '__main__':
    main()
