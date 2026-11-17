"""
"zipline" engine provider - the terminal's entry point (<command> --stdin, one JSON line on stdout).

WHAT RUNS (every result says it in `engine` / `engineImplementation` / assumptions / provenance)
  run_backtest   zl_engine.run_backtest: bars from base.price_store (exchange session dates); the
                 catalogue algorithm runs through the Fincept zipline-style API (zl_api:
                 initialize / handle_data / schedule_function / order_target_percent on full-history
                 indicators) and records its orders; base.simulator executes them (signals mode for
                 long / flat algorithms, weights mode for allocation algorithms) with the shared cost
                 model at the request's fillTiming; base.result_normalizer.build_backtest_result.
  optimize       base.robustness.run_optimization over zl_evaluate.evaluate (bars loaded once, the
                 algorithm's orders memoised per parameter set; seeded sampling, deflated Sharpe, PBO).
  walk_forward   base.robustness.run_walk_forward (re-optimised on every training window, warm-up
                 bars, tiling test windows, stitched out-of-sample equity).
  monte_carlo    base.monte_carlo over the run_backtest result.
  research       calculate_indicator / indicator_signals / indicator_param_sweep / generate_signals /
                 generate_labels / labels_to_signals / generate_splits / analyze_returns: the Fincept
                 research tools shared with the vectorbt engine (vectorbt\\vbt_research.py: price-store
                 bars, ONE indicator id table - the one the algorithms use, OHLC indicators, base.metrics,
                 base.simulator), labelled engine "zipline". v1's own copies downloaded from Yahoo with
                 UTC-shifted dates, computed CCI / Williams %R / ATR / ADX from closes only, returned
                 nothing for signal_filter, ignored rf / benchmark in analyze_returns and mislabelled
                 hindsight labels (audit Z4, Z9, Z10, Z18, Z19, Z20, Z32).

zipline-reloaded (installed: 3.2.1) is NOT imported and never was (audit Z1). A native path was
considered and not added: zipline runs only on an INGESTED bundle (an asset SQLite database plus
bcolz bar files, written per symbol set - they would have to live under base.paths.cache_root and be
rebuilt whenever the symbols or dates change), on ONE exchange calendar (XNYS by default: RELIANCE.NS
or 7203.T sessions would be dropped or misaligned - audit Z4 again), costs ~3-4 s of imports plus a
slow per-minute-style event loop per run, and its order / commission / slippage models (whole-share
order_target_percent, PerShare / PerDollar commissions, VolumeShareSlippage) are all covered by
base.costs + base.simulator, which every other engine shares. Its API is kept as this engine's
strategy language instead; get_capabilities reports the installed version and `used: false`.

Progress lines go to STDERR (@@BT_PROGRESS, base.progress); the host's cancelFile stops long loops
(exit code 3 + the cancelled envelope).
"""

import json
import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
_BACKTESTING_DIR = _SCRIPT_DIR.parent
# This folder is named `zipline/` (the host's path convention), which shadows the zipline-reloaded
# package for anything that puts the backtesting folder first on sys.path. Nothing here imports
# zipline-reloaded (its version is read from the distribution metadata), so the engine's own folders
# go first.
for _p in (str(_BACKTESTING_DIR), str(_SCRIPT_DIR)):
    if _p not in sys.path:
        sys.path.insert(0, _p)

RESEARCH_COMMANDS = ('calculate_indicator', 'indicator_signals', 'indicator_param_sweep', 'generate_signals',
                     'generate_labels', 'labels_to_signals', 'generate_splits', 'analyze_returns')
COMMANDS = ('run_backtest', 'optimize', 'walk_forward', 'monte_carlo', 'get_strategies', 'get_indicators',
            'get_command_options', 'get_capabilities', 'get_historical_data', 'test_import', 'test_connection',
            'initialize', 'disconnect') + RESEARCH_COMMANDS
RESEARCH_IMPLEMENTATION = ('Fincept research tools shared with the vectorbt engine (vbt_research: price-store bars, '
                           'vbt_indicators - the indicator table the zipline-style algorithms use, base.metrics, '
                           'base.simulator) - zipline-reloaded is not used')


def _vbt_path():
    vdir = str(_BACKTESTING_DIR / 'vectorbt')
    if vdir not in sys.path:
        sys.path.append(vdir)          # appended: this engine's own modules keep precedence


def _shared_research(command: str):
    """A research command served by the shared Fincept research tools (vectorbt\\vbt_research.py)."""
    def run(args):
        from base.base_provider import error_envelope
        _vbt_path()
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
            d['engine'] = 'zipline'
            d['engineImplementation'] = RESEARCH_IMPLEMENTATION
        return out
    return run


def get_strategies(_args):
    """The catalogue (vectorbt array shape). Every row: supported / runnable (every algorithm runs on
    base.simulator - no optional package needed) / reason (null); counts at the top level."""
    import zl_strategies as ST
    rows = ST.get_strategy_catalog()
    for r in rows:
        r['supported'] = True
        r['runnable'] = True
        r['reason'] = None
    return {'success': True, 'data': rows, 'provider': 'zipline', 'total': len(rows), 'supported': len(rows),
            'runnable_here': len(rows)}


def get_indicators(_args):
    _vbt_path()
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
    import zl_engine as E
    return {'success': True, 'data': {
        'position_sizing_methods': list(E.SIZING_MODES),
        'optimize_objectives': ['sharpe', 'sortino', 'calmar', 'return'] +
                               [o for o in OBJECTIVES if o not in ('sharpe', 'sortino', 'calmar', 'return')],
        'optimize_methods': ['grid', 'random'],
        'walk_forward_modes': ['rolling', 'anchored'],
        'fill_timings': list(FILL_MODES),
        'rebalance_periods': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly'],
        'label_types': ['FIXLB', 'MEANLB', 'LEXLB', 'TRENDLB', 'BOLB'],
        'splitter_types': ['RollingSplitter', 'ExpandingSplitter', 'PurgedKFoldSplitter'],
        'signal_generators': ['RAND', 'RANDX', 'RANDNX', 'RPROB', 'RPROBX'],
        'indicator_signal_modes': ['crossover_signals', 'threshold_signals', 'breakout_signals',
                                   'mean_reversion_signals', 'signal_filter'],
        'returns_analysis_types': ['returns_stats', 'drawdowns', 'ranges', 'rolling'],
    }}


REQUEST_KEYS = {
    'honoured': ['symbols', 'startDate', 'endDate', 'interval', 'initialCapital', 'commission', 'slippage',
                 'costModel', 'fillTiming', 'positionSizing', 'positionSizeValue', 'fractionalShares', 'stopLoss',
                 'takeProfit', 'trailingStop', 'allowShort', 'benchmarkSymbol', 'riskFreeRate', 'riskFreeSource',
                 'allowSyntheticData', 'brokerCandles', 'strategy', 'paramRanges', 'optimizeMethod',
                 'optimizeObjective', 'maxIterations', 'seed', 'holdoutFraction', 'maxWorkers', 'wfSplits',
                 'wfTrainRatio', 'wfMode', 'purgeBars', 'embargoBars', 'mcMethod', 'mcSimulations', 'mcSeed',
                 'mcBlockSize', 'mcRuinLevel', 'cancelFile'],
    'ignored': ['leverage'],
    'notes': {
        'strategy': 'a zipline-style algorithm (initialize / handle_data / schedule_function / order_target_percent) '
                    'decides; base.simulator executes - zipline-reloaded is not used',
        'symbols': 'every symbol is traded in ONE portfolio with shared cash (signal algorithms: equal slots of the '
                   'equity; pipeline / dual_momentum rank the symbols)',
        'fillTiming': 'next_open (default) | next_close | same_close, executed by base.simulator',
        'interval': 'any price-store interval; indicator lookbacks and schedules are in bars of that interval',
        'positionSizing': 'percent / fixed / risk (needs stopLoss) / vol_target for signal algorithms; kelly is not '
                          'implemented (IGNORED_SETTING, 100 % of the slot); allocation algorithms: percent scales '
                          'the target weights',
        'fractionalShares': 'default true (v1 behaviour); false = whole shares',
        'stopLoss': 'signal algorithms only (intrabar on high / low); ignored for allocation algorithms',
        'allowShort': 'signal algorithms only: a flat target opens a short (borrow fee from the cost model)',
        'costModel': 'pct_notional / per_share / per_trade commission with minimum and maximum; pct_price / '
                     'vol_scaled slippage + half spread on every fill price; borrow, cash interest, financing, '
                     'impact and participation cap as base.simulator models them',
        'leverage': 'positions are capped at available cash; a value other than 1 is listed under ignoredSettings '
                    'with an IGNORED_SETTING warning',
        'maxWorkers': 'a cap (also FINCEPT_BT_MAX_WORKERS); one process when the work is small',
        'rebalancePeriod': 'dual_momentum parameter: daily | weekly | monthly | quarterly | yearly (first bar of the '
                           'period, schedule_function date rule) or a number of bars'}}


def get_capabilities(_args):
    from base.base_provider import NOT_MODELLED_VOCABULARY, capabilities_payload
    from base.costs import PRESETS
    from base.execution import fill_model_block
    from base.monte_carlo import METHODS as MC_METHODS
    from base.price_store import INTERVALS
    import zl_engine as E
    import zl_strategies as ST
    modelled = {'borrow cost', 'cash interest', 'volume limits'}   # base.simulator
    zl_ok, zl_v = E.zipline_reloaded()
    data = capabilities_payload(
        'zipline', None, None, installed=True, commands=list(COMMANDS), fill_model=fill_model_block('next_open'),
        commission_models=['pct_notional', 'per_share', 'per_trade'], slippage_models=['pct_price', 'vol_scaled'],
        short_selling=True, leverage=False, multi_symbol=True, intraday=True, sizing_methods=list(E.SIZING_MODES),
        stops=True, monte_carlo_methods=list(MC_METHODS), optimize_methods=['grid', 'random'],
        walk_forward_modes=['rolling', 'anchored'],
        not_modelled=[x for x in NOT_MODELLED_VOCABULARY if x not in modelled],
        engine_implementation=E.short_implementation(), fill_timings=['next_open', 'next_close', 'same_close'],
        intervals=list(INTERVALS), cost_presets=sorted(PRESETS), request_keys=REQUEST_KEYS)
    data.update({'shares': 'fractional', 'fractionalShares': True, 'maxSymbolsPerRun': None, 'weightsBlock': True,
                 'stopTypes': ['stopLoss', 'takeProfit', 'trailingStop'], 'stopsApplyTo': 'signal algorithms',
                 'shortSellingAppliesTo': 'signal algorithms', 'costSensitivity': True,
                 'executors': {'signals': 'base.simulator (signals mode)',
                               'allocation': 'base.simulator (weights mode)'},
                 'tradeReporting': {'signals': 'round trips', 'allocation': 'position episodes'},
                 'strategyApi': 'Fincept zipline-style: initialize(context) / handle_data(context, data) / '
                                'schedule_function / order_target_percent on full-history indicators',
                 'algorithms': len(ST.STRATEGIES), 'rebalancePeriods': ['daily', 'weekly', 'monthly', 'quarterly',
                                                                        'yearly'],
                 'ziplineReloaded': {'installed': zl_ok, 'version': zl_v, 'used': False,
                                     'reason': 'needs ingested bundles on one exchange calendar; its order / cost '
                                               'models are covered by base.costs + base.simulator'}})
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


def _monte_carlo(args):
    from base.monte_carlo import run_monte_carlo
    import zl_engine as E

    def bt(req):
        try:
            return E.run_backtest(req)
        except Exception as e:  # surfaces as the Monte Carlo failure envelope
            return _error_from(e)
    return run_monte_carlo(bt, args)


def _error_from(e: BaseException):
    import traceback
    from base.base_provider import error_envelope
    import zl_engine as E
    try:
        from base.price_store import PriceDataError
    except Exception:  # pragma: no cover
        PriceDataError = RuntimeError
    if isinstance(e, E.EngineInputError):
        return error_envelope(str(e), 'INPUT')
    if isinstance(e, (E.EngineDataError, PriceDataError)):
        return error_envelope(str(e), 'DATA')
    if isinstance(e, ImportError):
        return error_envelope(f'A package the zipline engine needs is missing: {e}.', 'MISSING_PACKAGE')
    if isinstance(e, (ValueError, KeyError, TypeError)):
        return error_envelope(str(e) or type(e).__name__, 'INPUT', traceback_text=traceback.format_exc())
    return error_envelope(f'{type(e).__name__}: {e}', 'ENGINE', traceback_text=traceback.format_exc())


def dispatch(command: str, args: dict) -> dict:
    import zl_engine as E
    args = args if isinstance(args, dict) else {}
    zl_ok, zl_v = E.zipline_reloaded()
    simple = {'test_import': lambda a: {'success': True, 'version': E.ENGINE_VERSION, 'engine': 'zipline',
                                        'library': None, 'engineImplementation': E.short_implementation(),
                                        'ziplineReloaded': {'installed': zl_ok, 'version': zl_v, 'used': False}},
              'test_connection': lambda a: {'success': True, 'message': f'{E.short_implementation()} is available'},
              'initialize': lambda a: {'success': True, 'message': f'{E.short_implementation()} ready'},
              'disconnect': lambda a: {'success': True, 'message': 'Disconnected'}}
    table = {'run_backtest': E.run_backtest, 'optimize': E.optimize, 'walk_forward': E.walk_forward,
             'monte_carlo': _monte_carlo, 'get_strategies': get_strategies, 'get_indicators': get_indicators,
             'get_command_options': get_command_options, 'get_capabilities': get_capabilities,
             'get_historical_data': get_historical_data, **simple,
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
