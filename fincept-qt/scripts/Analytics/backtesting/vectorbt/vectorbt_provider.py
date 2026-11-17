"""
"VectorBT" engine provider - the terminal's entry point (<command> --stdin, one JSON line on stdout).

WHAT RUNS (every result says it in `engine` / `engineImplementation` / assumptions / provenance)
  run_backtest   vbt_engine.run_backtest: bars from base.price_store, signals from vbt_strategies,
                 simulation on base.simulator (one portfolio with shared cash; numba kernel when it
                 pays off, else the identical Python kernel), result from
                 base.result_normalizer.build_backtest_result.
  optimize       base.robustness.run_optimization over vbt_evaluate.evaluate (same simulator, costs,
                 fills and sizing as run_backtest; seeded sampling, deflated Sharpe, PBO).
  walk_forward   base.robustness.run_walk_forward (re-optimised on every training window).
  monte_carlo    base.monte_carlo over the run_backtest result.
  research       vbt_research (indicators, indicator signals, sweeps, signal generators, labels,
                 splits, returns analysis) on price-store bars.
The vectorbt LIBRARY (vectorbt 0.28.2 is installed) is NOT imported: the engine's own vectorised
pipeline runs instead and says so. This directory used to shadow the library (`import vectorbt`
imported this package) - nothing here imports `vectorbt` any more.

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
            'get_command_options', 'get_capabilities', 'get_historical_data', 'calculate_indicator',
            'indicator_signals', 'indicator_param_sweep', 'generate_signals', 'generate_labels', 'labels_to_signals',
            'generate_splits', 'analyze_returns', 'test_import', 'test_connection', 'initialize')


def _identity_text() -> str:
    import vbt_engine as E
    return E.implementation('numba')


def get_strategies(_args):
    import vbt_strategies as ST
    return {'success': True, 'data': ST.get_strategy_catalog()}


def get_indicators(_args):
    import vbt_indicators as I
    cat = I.catalog()
    modes = [
        {'mode': 'crossover_signals', 'usesIndicator': False,
         'description': 'two moving averages of the close (MA type and periods from the request)'},
        {'mode': 'threshold_signals', 'usesIndicator': True,
         'description': 'the selected oscillator crossing its lower / upper level (oscillators only)'},
        {'mode': 'breakout_signals', 'usesIndicator': False,
         'description': 'close vs the previous bar\'s donchian / bbands / keltner channel'},
        {'mode': 'mean_reversion_signals', 'usesIndicator': False,
         'description': 'z-score of the close crossing -z_entry / z_exit'},
        {'mode': 'signal_filter', 'usesIndicator': True,
         'description': 'threshold entries of the selected oscillator kept while a filter indicator is above / '
                        'below a level (oscillators only)'}]
    return {'success': True, 'indicators': cat, 'data': {'indicators': cat, 'signalModes': modes}}


def get_command_options(_args):
    from base.execution import FILL_MODES
    from base.robustness import OBJECTIVES
    import vbt_engine as E
    return {'success': True, 'data': {
        'position_sizing_methods': list(E.SIZING_MODES),
        'optimize_objectives': ['sharpe', 'sortino', 'calmar', 'return', 'omega'] +
                               [o for o in OBJECTIVES if o not in ('sharpe', 'sortino', 'calmar', 'return', 'omega')],
        'optimize_methods': ['grid', 'random'],
        'walk_forward_modes': ['rolling', 'anchored'],
        'fill_timings': list(FILL_MODES),
        'label_types': ['FIXLB', 'MEANLB', 'LEXLB', 'TRENDLB', 'BOLB'],
        'splitter_types': ['RollingSplitter', 'ExpandingSplitter', 'PurgedKFoldSplitter'],
        'signal_generators': ['RAND', 'RANDX', 'RANDNX', 'RPROB', 'RPROBX'],
        'indicator_signal_modes': ['crossover_signals', 'threshold_signals', 'breakout_signals',
                                   'mean_reversion_signals', 'signal_filter'],
        'returns_analysis_types': ['returns_stats', 'drawdowns', 'ranges', 'rolling'],
    }}


def get_capabilities(_args):
    from base.base_provider import capabilities_payload
    from base.execution import fill_model_block
    import vbt_engine as E
    from base.base_provider import NOT_MODELLED_VOCABULARY
    from base.costs import PRESETS
    from base.price_store import INTERVALS
    # base.simulator charges short borrow, accrues cash interest when costModel.cashRateAnnual is set and caps
    # fills at costModel.participationCap of the average volume - those three are modelled here.
    modelled = {'borrow cost', 'cash interest', 'volume limits'}
    data = capabilities_payload(
        'vectorbt', None, None, installed=True, commands=list(COMMANDS), fill_model=fill_model_block('next_open'),
        commission_models=['pct_notional', 'per_share', 'per_trade'], slippage_models=['pct_price', 'vol_scaled'],
        short_selling=True, leverage=False, multi_symbol=True, intraday=True, sizing_methods=list(E.SIZING_MODES),
        stops=True, optimize_methods=['grid', 'random'], walk_forward_modes=['rolling', 'anchored'],
        not_modelled=[x for x in NOT_MODELLED_VOCABULARY if x not in modelled],
        engine_implementation=_identity_text(), fill_timings=['next_open', 'next_close', 'same_close'],
        intervals=[iv for iv in INTERVALS], cost_presets=sorted(PRESETS),
        request_keys={
            'honoured': ['symbols', 'startDate', 'endDate', 'interval', 'initialCapital', 'commission', 'slippage',
                         'costModel', 'fillTiming', 'positionSizing', 'positionSizeValue', 'stopLoss', 'takeProfit',
                         'trailingStop', 'allowShort', 'benchmarkSymbol', 'riskFreeRate', 'riskFreeSource',
                         'allowSyntheticData', 'strategy', 'paramRanges', 'optimizeMethod', 'optimizeObjective',
                         'maxIterations', 'seed', 'holdoutFraction', 'maxWorkers', 'wfSplits', 'wfTrainRatio',
                         'wfMode', 'purgeBars', 'embargoBars', 'mcMethod', 'mcSimulations', 'mcSeed', 'mcBlockSize',
                         'mcRuinLevel', 'cancelFile'],
            'ignored': ['leverage'],
            'notes': {'leverage': 'long positions are capped by cash (no margin); a value other than 1 is listed '
                                  'under ignoredSettings with an IGNORED_SETTING warning',
                      'allowShort': 'exit signals open shorts; borrow fee from the cost model',
                      'maxWorkers': 'capped by FINCEPT_BT_MAX_WORKERS; one process when the work is small'}})
    return {'success': True, 'data': data}


def get_historical_data(args):
    from base.price_store import get_bars
    syms = args.get('symbols') or []
    res = get_bars(syms, str(args.get('startDate'))[:10], str(args.get('endDate'))[:10],
                   interval=str(args.get('interval') or '1d'), progress=False)
    from base.metrics import _fmt_date
    out = []
    for s in syms:
        from base.price_store import normalize_symbol
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


def _monte_carlo(args):
    from base.monte_carlo import run_monte_carlo
    import vbt_engine as E

    def bt(req):
        try:
            return E.run_backtest(req)
        except Exception as e:  # surfaces as the Monte Carlo failure envelope
            return _error_from(e)
    return run_monte_carlo(bt, args)


def _error_from(e: BaseException):
    import traceback
    from base.base_provider import error_envelope
    import vbt_engine as E
    import vbt_research as RS
    try:
        from base.price_store import PriceDataError
    except Exception:  # pragma: no cover
        PriceDataError = RuntimeError
    if isinstance(e, (E.EngineInputError, RS.ResearchInputError)):
        return error_envelope(str(e), 'INPUT')
    if isinstance(e, (E.EngineDataError, PriceDataError)):
        return error_envelope(str(e), 'DATA')
    if isinstance(e, (ValueError, KeyError, TypeError)):
        return error_envelope(str(e) or type(e).__name__, 'INPUT', traceback_text=traceback.format_exc())
    return error_envelope(f'{type(e).__name__}: {e}', 'ENGINE', traceback_text=traceback.format_exc())


def dispatch(command: str, args: dict) -> dict:
    import vbt_engine as E
    import vbt_research as RS
    simple = {'test_import': lambda a: {'success': True, 'version': E.ENGINE_VERSION, 'engine': 'vectorbt',
                                        'library': None, 'engineImplementation': _identity_text()},
              'test_connection': lambda a: {'success': True, 'message': f'{_identity_text()} is available'},
              'initialize': lambda a: {'success': True, 'message': f'{_identity_text()} ready'}}
    table = {
        'run_backtest': E.run_backtest, 'optimize': E.optimize, 'walk_forward': E.walk_forward,
        'monte_carlo': _monte_carlo, 'get_strategies': get_strategies, 'get_indicators': get_indicators,
        'get_command_options': get_command_options, 'get_capabilities': get_capabilities,
        'get_historical_data': get_historical_data, 'calculate_indicator': RS.calculate_indicator,
        'indicator_signals': RS.indicator_signals, 'indicator_param_sweep': RS.indicator_param_sweep,
        'generate_signals': RS.generate_signals, 'generate_labels': RS.generate_labels,
        'labels_to_signals': RS.labels_to_signals, 'generate_splits': RS.generate_splits,
        'analyze_returns': RS.analyze_returns, **simple}
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
