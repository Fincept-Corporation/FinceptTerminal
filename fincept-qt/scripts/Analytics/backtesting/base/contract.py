"""
Result contract v2 as data, and a validator (audit Part 6; the gate for every engine batch).

    from base.contract import validate
    problems = validate('run_backtest', payload_data)     # [] when the result conforms

Type codes (a trailing '?' means null is allowed; 'opt:' prefix on a key = the key may be absent):
  num      finite number            int     integer            str     string        bool  boolean
  frac     number in [0, 1]         pos     number >= 0        neg     number <= 0
  date     'YYYY-MM-DD' or 'YYYY-MM-DD HH:MM:SS'   obj  JSON object    list  JSON array
  {..}     nested object spec       [spec]  list whose elements follow spec    ('enum', a, b) one of
Global rules: no NaN / Infinity anywhere; field names camelCase (no '_') except inside data
maps (parameters, bestParameters, bestParams, params, paramGrid, paramStability,
parameterSensitivity, parameterSpread, priceData); equity dates strictly increasing; trades:
closed in entry order, open positions last with exitDate null and exitReason 'open'.

UNITS: returns, rates, weights, exposure, drawdowns are FRACTIONS; maxDrawdown and
drawdowns[].depth POSITIVE; equity[].drawdown <= 0; VaR / CVaR are bar RETURNS (negative = loss); money in the
account currency; holdingPeriod / *Bars in bars of the run's interval; ulcerIndex in percent
points (v1 unit, R5) and ulcerIndexFraction as a fraction.
"""

from __future__ import annotations

import math
import re
from typing import Any, Dict, List

_DATE = re.compile(r'^\d{4}-\d{2}-\d{2}( \d{2}:\d{2}:\d{2})?$')
_DATA_MAPS = {'parameters', 'bestParameters', 'bestParams', 'params', 'paramGrid', 'paramStability',
              'parameterSensitivity', 'parameterSpread', 'priceData', 'undefinedMetrics', 'engineReported',
              'paramRanges'}
EXIT_REASONS = ('signal', 'stop_loss', 'take_profit', 'trailing_stop', 'end_of_data', 'rebalance', 'open',
                'time_limit', 'margin_call')
NOT_MODELLED_VOCAB = ('bid/ask spread', 'market impact', 'partial fills', 'volume limits', 'borrow cost',
                      'margin calls', 'cash interest', 'dividend withholding', 'FX conversion', 'trading halts',
                      'delisted symbols')
SEVERITIES = ('info', 'warn', 'error')

WARNING = {'code': 'str', 'severity': ('enum',) + SEVERITIES, 'message': 'str', 'symbol': 'str?'}

PERFORMANCE = {
    'totalReturn': 'num', 'annualizedReturn': 'num?', 'cagr': 'num?', 'volatility': 'pos?', 'sharpeRatio': 'num?',
    'sortinoRatio': 'num?', 'calmarRatio': 'num?', 'maxDrawdown': 'frac', 'maxDrawdownDuration': 'int',
    'maxDrawdownDurationDays': 'int?', 'downsideDeviation': 'pos?', 'alpha': 'num?', 'beta': 'num?',
    'informationRatio': 'num?', 'trackingError': 'pos?', 'treynorRatio': 'num?', 'exposureTime': 'frac?',
    'recoveryFactor': 'num?', 'totalTrades': 'int', 'winningTrades': 'int', 'losingTrades': 'int',
    'winRate': 'frac?', 'lossRate': 'frac?', 'profitFactor': 'pos?',
    'opt:averageWin': 'num?', 'opt:averageLoss': 'num?', 'opt:largestWin': 'num?', 'opt:largestLoss': 'num?',
    'opt:expectancy': 'num?', 'opt:averageTradeReturn': 'num?',
}
STATISTICS = {
    'startDate': 'date', 'endDate': 'date', 'initialCapital': 'pos', 'finalCapital': 'num', 'totalFees': 'pos?',
    'totalSlippage': 'pos?', 'totalTrades': 'int', 'winningDays': 'int', 'losingDays': 'int',
    'averageDailyReturn': 'num?', 'bestDay': 'num?', 'worstDay': 'num?', 'consecutiveWins': 'int',
    'consecutiveLosses': 'int', 'maxDrawdownDate': 'date?', 'recoveryTime': 'int?',
}
EQUITY_ROW = {'date': 'date', 'equity': 'num', 'returns': 'num', 'drawdown': 'neg', 'benchmark': 'num?',
              'opt:exposure': 'pos?', 'opt:netExposure': 'num?', 'opt:cash': 'num?'}
TRADE = {
    'id': 'str', 'symbol': 'str', 'side': ('enum', 'long', 'short'), 'entryDate': 'date', 'entryPrice': 'num',
    'exitDate': 'date?', 'exitPrice': 'num?', 'quantity': 'pos', 'pnl': 'num?', 'pnlPercent': 'num?',
    'grossPnl': 'num?', 'commission': 'pos', 'slippageCost': 'pos?', 'holdingPeriod': 'int?',
    'opt:holdingDays': 'int?', 'mae': 'neg?', 'mfe': 'pos?', 'exitReason': ('enum',) + EXIT_REASONS,
    'entryBar': 'int?', 'exitBar': 'int?', 'opt:markPrice': 'num?', 'opt:borrowCost': 'pos?', 'opt:dividends': 'num?',
    'opt:unrealized': 'bool',
}
TRADE_STATS = {k: t for k, t in {
    'closedTrades': 'int', 'openTrades': 'int', 'winningTrades': 'int', 'losingTrades': 'int', 'winRate': 'frac?',
    'grossProfit': 'pos', 'grossLoss': 'neg', 'netPnl': 'num', 'netPnlPct': 'num?', 'profitFactor': 'pos?',
    'profitFactorInfinite': 'bool', 'avgWin': 'num?', 'avgLoss': 'num?', 'avgWinPct': 'num?', 'avgLossPct': 'num?',
    'largestWin': 'num?', 'largestLoss': 'num?', 'payoffRatio': 'num?', 'expectancy': 'num?',
    'expectancyPct': 'num?', 'avgHoldingBars': 'num?', 'avgWinHoldingBars': 'num?', 'avgLossHoldingBars': 'num?',
    'maxConsecutiveWins': 'int', 'maxConsecutiveLosses': 'int', 'totalCommission': 'pos', 'longTrades': 'int',
    'shortTrades': 'int', 'longWinRate': 'frac?', 'shortWinRate': 'frac?', 'avgMae': 'neg?', 'avgMfe': 'pos?',
    'worstMae': 'neg?', 'bestMfe': 'pos?', 'sqn': 'num?', 'kellyFraction': 'num?', 'openPnl': 'num?'}.items()}
ADVANCED = {'var95': 'num?', 'var99': 'num?', 'cvar95': 'num?', 'cvar99': 'num?', 'ulcerIndex': 'pos?',
            'ulcerIndexFraction': 'pos?', 'omegaRatio': 'pos?', 'tailRatio': 'pos?', 'skewness': 'num?',
            'kurtosis': 'num?', 'benchmarkAlpha': 'num?', 'benchmarkBeta': 'num?', 'benchmarkCorrelation': 'num?',
            'rSquared': 'frac?', 'informationRatio': 'num?', 'trackingError': 'pos?', 'upCapture': 'num?',
            'downCapture': 'num?', 'returnsHistogram': 'list'}
DATED = {'date': 'date', 'value': 'num'}
DRAWDOWN = {'rank': 'int', 'depth': 'frac', 'peakDate': 'date', 'troughDate': 'date', 'recoveryDate': 'date?',
            'toTroughBars': 'int', 'recoveryBars': 'int?', 'durationBars': 'int', 'durationDays': 'int',
            'peakEquity': 'num', 'troughEquity': 'num'}
COSTS = {'totalCommission': 'pos?', 'totalSlippage': 'pos?', 'totalBorrow': 'pos?', 'totalCosts': 'pos?',
         'grossPnl': 'num?', 'netPnl': 'num', 'costDrag': 'num?', 'tradedNotional': 'pos', 'annualTurnover': 'pos?',
         'fills': 'int', 'costsPctOfGrossProfit': 'pos?',
         'byYear': [{'year': 'int', 'commission': 'pos', 'slippage': 'pos?', 'tradedNotional': 'pos',
                     'turnover': 'pos?'}],
         'model': 'obj?'}
EXPOSURE = {k: 'num?' for k in ('timeInMarket', 'avgGrossExposure', 'maxGrossExposure', 'avgNetExposure',
                                'avgPositions', 'maxPositions', 'longShare', 'shortShare')}
ATTRIBUTION = {'symbol': 'str', 'trades': 'int', 'winRate': 'frac?', 'netPnl': 'num', 'grossPnl': 'num?',
               'costs': 'pos', 'contribution': 'num?', 'avgHoldingBars': 'num?', 'timeInMarket': 'frac?'}
POSITION = {'symbol': 'str', 'quantity': 'num', 'avgPrice': 'num?', 'lastPrice': 'num', 'marketValue': 'num',
            'weight': 'num?', 'unrealizedPnl': 'num?'}
FILL_MODEL = {'signalAt': ('enum', 'close'), 'fillAt': ('enum', 'next_open', 'next_close', 'same_close'),
              'description': 'str'}
ASSUMPTIONS = {'engine': 'str', 'execution': 'str', 'riskFreeRate': 'num', 'riskFreeSource': 'str',
               'fillModel': FILL_MODEL, 'notModelled': 'list', 'opt:costs': 'str?', 'opt:dataSource': 'str?',
               'opt:annualization': 'str?', 'opt:ignoredSettings': 'list', 'opt:notes': 'list'}
PROV_SYMBOL = {'symbol': 'str', 'bars': 'int?', 'firstBar': 'date?', 'lastBar': 'date?', 'currency': 'str?',
               'missingBars': 'int?', 'source': 'str?', 'cacheAsOf': 'str?'}
PROVENANCE = {'engine': 'str', 'engineImplementation': 'str', 'engineVersion': 'str?', 'library': 'str?',
              'libraryVersion': 'str?', 'pythonVersion': 'str', 'runId': 'str', 'startedAt': 'str',
              'durationMs': 'int', 'dataSource': 'str', 'adjustment': 'str', 'interval': 'str', 'bars': 'int',
              'firstBar': 'date', 'lastBar': 'date', 'requestedStart': 'date?', 'requestedEnd': 'date?',
              'warmupBars': 'int?', 'strategy': 'obj', 'parameters': 'obj', 'symbols': [PROV_SYMBOL]}

RUN_BACKTEST = {
    'schemaVersion': 'int', 'id': 'str', 'status': 'str', 'usingSyntheticData': 'bool', 'periodsPerYear': 'pos',
    'engine': 'str', 'engineImplementation': 'str', 'engineVersion': 'str?',
    'performance': PERFORMANCE, 'statistics': STATISTICS, 'equity': [EQUITY_ROW], 'trades': [TRADE],
    'tradeStats': TRADE_STATS, 'advancedMetrics': ADVANCED,
    'monthlyReturns': [{'year': 'int', 'months': 'list', 'yearTotal': 'num?'}],
    'opt:benchmarkMonthlyReturns': [{'year': 'int', 'months': 'list', 'yearTotal': 'num?'}],
    'yearlyReturns': [{'year': 'int', 'strategy': 'num?', 'benchmark': 'num?', 'excess': 'num?'}],
    'rollingMetrics': {'window': 'int', 'rollingSharpe': [DATED], 'rollingVolatility': [DATED],
                       'rollingDrawdown': [DATED], 'opt:rollingBeta': [DATED]},
    'periodStats': 'obj', 'drawdowns': [DRAWDOWN], 'attribution': [ATTRIBUTION], 'exposure': EXPOSURE,
    'positions': [POSITION], 'costs': COSTS, 'provenance': PROVENANCE, 'assumptions': ASSUMPTIONS,
    'warnings': [WARNING], 'logs': 'list',
    'opt:benchmark': {'symbol': 'str?', 'totalReturn': 'num', 'annualizedReturn': 'num?', 'volatility': 'pos?',
                      'maxDrawdown': 'frac', 'sharpeRatio': 'num?', 'sortinoRatio': 'num?'},
    'opt:fills': [{'date': 'date', 'symbol': 'str', 'side': ('enum', 'buy', 'sell'), 'quantity': 'pos',
                   'price': 'num?', 'commission': 'pos', 'slippageCost': 'pos?'}],
    'opt:priceData': 'obj',
}
OVERFITTING = {'nTrials': 'int', 'sharpeStd': 'pos?', 'expectedMaxSharpe': 'num?', 'deflatedSharpe': 'frac?',
               'pbo': 'frac?', 'effectiveTrials': 'pos?'}
PERF_ROW = {'opt:totalReturn': 'num?', 'opt:sharpeRatio': 'num?', 'opt:maxDrawdown': 'num?', 'opt:totalTrades': 'num?'}
OPTIMIZE = {
    'schemaVersion': 'int', 'objective': 'str', 'method': 'str', 'iterations': 'int', 'totalCombinations': 'int',
    'bestParameters': 'obj', 'bestObjectiveValue': 'num',
    'allResults': [{'parameters': 'obj', 'objectiveValue': 'num', 'performance': PERF_ROW}],
    'paramNames': 'list', 'parameterSensitivity': 'obj', 'parameterSpread': 'obj', 'overfitting': OVERFITTING,
    'opt:truncated': 'bool', 'opt:bestPerformance': 'obj', 'opt:paramGrid': 'obj', 'opt:stability': 'obj',
    'opt:holdout': 'obj', 'opt:statisticalSignificance': {'pValue': 'frac', 'significant': 'bool'},
    'opt:warnings': [WARNING],
}
WF_WINDOW = {'window': 'int', 'opt:trainStart': 'date?', 'opt:trainEnd': 'date?', 'testStart': 'date?',
             'testEnd': 'date?', 'opt:bestParams': 'obj?', 'opt:inSampleReturn': 'num?', 'opt:inSampleSharpe': 'num?',
             'outOfSampleReturn': 'num?', 'opt:outOfSampleSharpe': 'num?', 'opt:outOfSampleMaxDrawdown': 'num?',
             'opt:outOfSampleTrades': 'num?'}
WALK_FORWARD = {
    'schemaVersion': 'int', 'nWindows': 'int', 'windows': [WF_WINDOW], 'opt:oosEquity': [{'date': 'str',
                                                                                          'equity': 'num'}],
    'avgOosReturn': 'num?', 'opt:avgOosSharpe': 'num?', 'oosReturnStd': 'num?', 'compoundedOosReturn': 'num?',
    'robustnessScore': 'frac?', 'degradationRatio': 'num?', 'opt:walkForwardEfficiency': 'num?',
    'mode': ('enum', 'rolling', 'anchored', None), 'opt:purgeBars': 'int?', 'opt:embargoBars': 'int?',
    'opt:warnings': [WARNING],
}
MONTE_CARLO = {
    'schemaVersion': 'int', 'method': 'str', 'methodId': ('enum', 'trade_bootstrap', 'trade_shuffle',
                                                         'returns_bootstrap', 'block_bootstrap'),
    'nSimulations': 'int', 'seed': 'int', 'blockSize': 'num?', 'horizonBars': 'int?', 'nTrades': 'int',
    'initialCapital': 'pos', 'positionFraction': 'pos?', 'x': 'list',
    'bands': [{'lower': 'num', 'upper': 'num', 'lowerValues': 'list', 'upperValues': 'list'}],
    'percentileBands': {'x': 'list', 'p5': 'list', 'p25': 'list', 'p50': 'list', 'p75': 'list', 'p95': 'list'},
    'median': 'list', 'originalPath': 'list', 'samplePaths': 'list', 'finalEquity': 'list', 'maxDrawdown': 'list',
    'finalValues': 'list', 'drawdownValues': 'list', 'tradeReturns': 'list',
    'stats': {'finalP5': 'num?', 'finalP25': 'num?', 'finalP50': 'num?', 'finalP75': 'num?', 'finalP95': 'num?',
              'finalMean': 'num?', 'originalFinal': 'num', 'originalPercentile': 'frac?', 'probLoss': 'frac',
              'probRuin': 'frac', 'ruinLevel': 'frac', 'ddP50': 'frac', 'ddP95': 'frac', 'ddP99': 'frac',
              'originalMaxDrawdown': 'frac', 'cagrP5': 'num?', 'cagrP50': 'num?', 'cagrP95': 'num?'},
    'warnings': [WARNING], 'assumptions': 'obj', 'logs': 'list',
}
SPECS = {'run_backtest': RUN_BACKTEST, 'execute_fincept_strategy': RUN_BACKTEST, 'optimize': OPTIMIZE,
         'walk_forward': WALK_FORWARD, 'monte_carlo': MONTE_CARLO}


def _is_num(v) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(float(v))


def _check(v, spec, path: str, errs: List[str]) -> None:
    if isinstance(spec, dict):
        if not isinstance(v, dict):
            errs.append(f'{path}: expected object, got {type(v).__name__}')
            return
        for k, sub in spec.items():
            opt = k.startswith('opt:')
            key = k[4:] if opt else k
            if key not in v:
                if not opt:
                    errs.append(f'{path}.{key}: missing')
                continue
            _check(v[key], sub, f'{path}.{key}', errs)
        return
    if isinstance(spec, list):
        if not isinstance(v, list):
            errs.append(f'{path}: expected array, got {type(v).__name__}')
            return
        for i, item in enumerate(v[:5000]):
            _check(item, spec[0], f'{path}[{i}]', errs)
            if len(errs) > 200:
                return
        return
    if isinstance(spec, tuple) and spec and spec[0] == 'enum':
        if v not in spec[1:]:
            errs.append(f'{path}: {v!r} not one of {spec[1:]}')
        return
    t = str(spec)
    nullable = t.endswith('?')
    t = t.rstrip('?')
    if v is None:
        if not nullable:
            errs.append(f'{path}: null not allowed')
        return
    ok = True
    if t == 'num':
        ok = _is_num(v)
    elif t == 'int':
        ok = isinstance(v, int) and not isinstance(v, bool)
    elif t == 'str':
        ok = isinstance(v, str)
    elif t == 'bool':
        ok = isinstance(v, bool)
    elif t == 'obj':
        ok = isinstance(v, dict)
    elif t == 'list':
        ok = isinstance(v, list)
    elif t == 'frac':
        ok = _is_num(v) and -1e-9 <= float(v) <= 1 + 1e-9
    elif t == 'pos':
        ok = _is_num(v) and float(v) >= -1e-9
    elif t == 'neg':
        ok = _is_num(v) and float(v) <= 1e-12
    elif t == 'date':
        ok = isinstance(v, str) and bool(_DATE.match(v))
    if not ok:
        errs.append(f'{path}: {v!r} is not {t}')


def _walk_global(v, path: str, errs: List[str], in_map: bool = False) -> None:
    if isinstance(v, float) and not math.isfinite(v):
        errs.append(f'{path}: non-finite number')
    elif isinstance(v, dict):
        for k, x in v.items():
            if not in_map and isinstance(k, str) and '_' in k:
                errs.append(f'{path}.{k}: field name is not camelCase')
            _walk_global(x, f'{path}.{k}', errs, in_map=(k in _DATA_MAPS))
            if len(errs) > 200:
                return
    elif isinstance(v, list):
        for i, x in enumerate(v[:2000]):
            _walk_global(x, f'{path}[{i}]', errs, in_map=False)


def validate(command: str, data: Dict[str, Any]) -> List[str]:
    """Problems of a result's `data` against contract v2 ([] = conforms)."""
    spec = SPECS.get(command)
    if spec is None:
        return [f'no contract for command {command!r}']
    errs: List[str] = []
    if not isinstance(data, dict):
        return ['data: not an object']
    if data.get('schemaVersion') != 2:
        errs.append('schemaVersion: must be 2')
    _check(data, spec, 'data', errs)
    _walk_global(data, 'data', errs)
    if command in ('run_backtest', 'execute_fincept_strategy'):
        eq = data.get('equity') or []
        ds = [p.get('date') for p in eq if isinstance(p, dict)]
        if any(a >= b for a, b in zip(ds, ds[1:]) if isinstance(a, str) and isinstance(b, str)):
            errs.append('equity: dates are not strictly increasing')
        trades = data.get('trades') or []
        seen_open = False
        for i, t in enumerate(trades):
            if not isinstance(t, dict):
                continue
            is_open = t.get('exitDate') is None
            if is_open and t.get('exitReason') != 'open':
                errs.append(f'trades[{i}]: open position must have exitReason "open"')
            if not is_open and seen_open:
                errs.append(f'trades[{i}]: closed trade listed after an open position')
            seen_open = seen_open or is_open
        nm = (data.get('assumptions') or {}).get('notModelled') or []
        bad = [x for x in nm if x not in NOT_MODELLED_VOCAB]
        if bad:
            errs.append(f'assumptions.notModelled: unknown entries {bad}')
        months = data.get('monthlyReturns') or []
        for i, m in enumerate(months):
            if isinstance(m, dict) and len(m.get('months') or []) != 12:
                errs.append(f'monthlyReturns[{i}].months: must have 12 entries')
    if command == 'optimize':
        objs = [r.get('objectiveValue') for r in data.get('allResults') or [] if isinstance(r, dict)]
        if any(_is_num(a) and _is_num(b) and a < b for a, b in zip(objs, objs[1:])):
            errs.append('allResults: not sorted best first')
    return errs


def validate_envelope(obj: Dict[str, Any], command: str) -> List[str]:
    """Envelope (R2) + data validation of one parsed stdout line."""
    if not isinstance(obj, dict) or 'success' not in obj:
        return ['envelope: missing success']
    if obj['success'] is False:
        errs = []
        if not isinstance(obj.get('error'), str) or not obj['error']:
            errs.append('error: missing message')
        if obj.get('errorCode') not in ('DATA', 'INPUT', 'ENGINE', 'MISSING_PACKAGE', 'CANCELLED', 'TIMEOUT',
                                        'INTERNAL'):
            errs.append(f'errorCode: {obj.get("errorCode")!r}')
        if 'Traceback' in str(obj.get('error')):
            errs.append('error: contains a traceback (belongs in details.traceback)')
        return errs
    return validate(command, obj.get('data'))
