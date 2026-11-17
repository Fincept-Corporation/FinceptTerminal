"""
The "fincept" engine on the shared core: Fincept's own QCAlgorithm-compatible bar-loop engine
(scripts/strategies/fincept_engine) runs the strategy's code; base/ supplies data, metrics,
robustness and the result contract.

WHAT RUNS
  run_backtest   base/fincept_strategy_runner.py loads the registry strategy and runs it bar by bar in
                 fincept_engine (session loop, broker fills, margin, options / futures / universes as the
                 strategy subscribes them). It is the executor - base.simulator is NOT used. Bars come from
                 base.price_store through fincept_engine.price_feed. The result is built by
                 base.result_normalizer.build_backtest_result with this engine's identity (engine 'fincept',
                 engineImplementation IMPLEMENTATION, fill mode = the fill timing that really ran).
  optimize       base.robustness.run_optimization over fincept_evaluate.evaluate: every combination is a full
                 engine run with the strategy parameters set (QCAlgorithm.get_parameter); bars are read once
                 per process (price_feed memo). Strategies that read no parameters are refused (INPUT).
  walk_forward   base.robustness.run_walk_forward: re-optimised on every training window, the winner run on
                 its test window (tiling test windows, stitched out-of-sample equity, rolling / anchored,
                 purge / embargo); each window run seeds the strategy's indicators with the bars before it
                 (the engine's automatic indicator warm-up, from the 730 days of history every run loads).
  monte_carlo    base.monte_carlo over run_backtest.

FILLS. The broker's native rule: a MARKET order placed after bar t's close fills at bar t+1's OPEN
('next_open'); fillTiming 'next_close' / 'same_close' move market orders to the next / same close;
limit, stop, trailing-stop, MOO / MOC and limit-if-touched orders keep their own rules (against each
bar's range) whatever the fill timing.

COSTS. base.costs.CostModel.from_request mapped onto the broker: commission pct of notional / per
share / per trade with minimum and maximum, slippage pct of price / volatility-scaled + half the
quoted spread, short borrow fee accrued daily. Market impact, participation caps, cash interest and
financing are not applied (IGNORED_SETTING). A fee / slippage / fill model the strategy sets on a
security wins for that security (noted). Commission, slippage and borrow are itemised per fill,
per trade and in total.
"""

from __future__ import annotations

import math
import os
import sys
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime
from typing import Any, Dict, List, Optional, Tuple

_HERE = os.path.dirname(os.path.abspath(__file__))
_BT = os.path.dirname(_HERE)
for _p in (_BT, os.path.join(_BT, 'base'), _HERE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

ENGINE = 'fincept'
ENGINE_VERSION = '2.0.0'
IMPLEMENTATION = ('Fincept bar-loop engine (QCAlgorithm-compatible): the strategy\'s own code runs bar by bar in '
                  'strategies/fincept_engine and its broker fills the orders; base.simulator is not used')
SHORT_IMPLEMENTATION = 'Fincept bar-loop engine (QCAlgorithm-compatible)'
SENSITIVITY_NOTE = ('costs.sensitivity is not computed for this engine: each cost multiple would be another full run '
                    'of the strategy\'s code (seconds to minutes), not a cheap re-simulation of fixed orders.')
NATIVE_FILL_TEXT = ('The strategy sees each bar after it closes (indicators, consolidators and on_data are updated with '
                    'that close). MARKET orders fill at the next bar\'s {at}; MARKET_ON_OPEN at the next open; '
                    'MARKET_ON_CLOSE at the next close; LIMIT / STOP orders on a later bar whose range reaches the '
                    'price (at the price, or at the open when it gaps through); trailing-stop and limit-if-touched '
                    'orders against each bar\'s range; combo orders fill all legs together or not at all. Scheduled '
                    'events before the close see only the previous close and their market orders fill at that day\'s '
                    'open.')
_IGNORED_COST_FIELDS = (('impact_coef', 'costModel.impactCoef (market impact is not modelled by this engine)'),
                        ('participation_cap', 'costModel.participationCap (fills are not capped by volume)'),
                        ('cash_rate_annual', 'costModel.cashRateAnnual (no interest is paid on cash)'),
                        ('financing_rate_annual', 'costModel.financingRateAnnual (margin financing is not charged '
                                                  'from the cost model; a strategy\'s own margin-interest model is)'))


class EngineInputError(ValueError):
    pass


class EngineDataError(RuntimeError):
    pass


class EngineRunError(RuntimeError):
    """The strategy failed inside the engine (or needs a feature the engine does not have)."""

    def __init__(self, message: str, code: str = 'ENGINE', traceback_text: Optional[str] = None,
                 not_supported: bool = False, feature: Optional[str] = None):
        super().__init__(message)
        self.code = code
        self.traceback_text = traceback_text
        self.not_supported = not_supported
        self.feature = feature


def _log(msg: str) -> None:
    print(f'[fincept] {msg}', file=sys.stderr)


def _num(v, default=None):
    try:
        x = float(v)
        return x if math.isfinite(x) else default
    except (TypeError, ValueError):
        return default


# ════════════════════════════════════════════════════════════════════════════
# Request
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class Spec:
    strategy_id: str
    strategy_name: str
    symbols: List[str]
    start: str
    end: str
    cash: float
    params: Dict[str, Any]
    cost_model: Any
    fill: str
    ignored: List[str] = field(default_factory=list)
    interval: str = '1d'


_RUNNER = None


def runner():
    global _RUNNER
    if _RUNNER is None:
        from fincept_strategy_runner import FinceptStrategyRunner
        _RUNNER = FinceptStrategyRunner()
    return _RUNNER


def parse_request(req: Dict[str, Any]) -> Spec:
    from base.costs import CostModel
    from base.execution import fill_mode_from_request
    strat = req.get('strategy') if isinstance(req.get('strategy'), dict) else {}
    sid = str(strat.get('type') or strat.get('id') or '').strip()
    if not sid:
        raise EngineInputError('strategy.type (the strategy id, e.g. FCT-B8F7654B) is required.')
    info = runner().get_strategy_info(sid)
    if not info:
        raise EngineInputError(f'Unknown strategy {sid}: it is not in the Fincept strategy registry.')
    params = strat.get('params') or strat.get('parameters') or {}
    if not isinstance(params, dict):
        raise EngineInputError('strategy.params must be an object of parameter values.')
    symbols = [str(s).strip() for s in (req.get('symbols') or []) if str(s).strip()]
    if not symbols and isinstance(req.get('assets'), list):
        symbols = [str(a.get('symbol')).strip() for a in req['assets'] if isinstance(a, dict) and a.get('symbol')]
    if not symbols:
        raise EngineInputError('No symbols specified (symbols: ["SPY", ...]).')
    start = str(req.get('startDate') or '')[:10]
    end = str(req.get('endDate') or '')[:10]
    if not end:
        from datetime import timedelta
        end = (datetime.now() - timedelta(days=1)).strftime('%Y-%m-%d')
    if not start:
        raise EngineInputError('startDate is required (YYYY-MM-DD).')
    try:
        if datetime.strptime(end, '%Y-%m-%d') < datetime.strptime(start, '%Y-%m-%d'):
            raise EngineInputError(f'endDate {end} is before startDate {start}.')
    except ValueError as e:
        if isinstance(e, EngineInputError):
            raise
        raise EngineInputError(f'Dates must be YYYY-MM-DD (got {start!r} / {end!r}).')
    cash = _num(req.get('initialCapital'), 100000.0) or 100000.0
    if cash <= 0:
        raise EngineInputError('initialCapital must be positive.')
    try:
        fill = fill_mode_from_request(req)
    except ValueError as e:
        raise EngineInputError(str(e))
    try:
        cost_model = CostModel.from_request(req, symbol=symbols[0])
    except ValueError as e:
        raise EngineInputError(f'costModel: {e}')
    ignored = []
    for attr, text in _IGNORED_COST_FIELDS:
        v = getattr(cost_model, attr, None)
        if v not in (None, 0, 0.0):
            ignored.append(text)
    for k, text in (('stopLoss', 'stop-loss'), ('takeProfit', 'take-profit'), ('trailingStop', 'trailing stop')):
        if (_num(req.get(k), 0.0) or 0.0) > 0:
            ignored.append(f'{text} (the strategy code manages its own exits)')
    if str(req.get('positionSizing') or 'percent') != 'percent' or (_num(req.get('positionSizeValue'), 100.0)
                                                                    or 100.0) != 100.0:
        ignored.append('position sizing (the strategy code sizes its own orders)')
    if (_num(req.get('leverage'), 1.0) or 1.0) != 1.0:
        ignored.append('leverage (each security uses the strategy\'s own leverage, capped per asset class)')
    interval = str(req.get('interval') or '1d')
    if interval not in ('1d', 'd', 'daily', '1day'):
        ignored.append(f'interval {interval} (the Fincept engine steps through daily bars)')
    return Spec(sid, info['name'], symbols, start, end, float(cash), dict(params), cost_model, fill, ignored,
                interval)


# ════════════════════════════════════════════════════════════════════════════
# One engine run
# ════════════════════════════════════════════════════════════════════════════

def run_engine(spec: Spec, params: Optional[Dict[str, Any]] = None, start: Optional[str] = None,
               end: Optional[str] = None, on_bar=None, auto_warm: bool = False) -> Dict[str, Any]:
    """The runner's raw result data for one run; raises EngineRunError / EngineInputError / EngineDataError."""
    r = runner()
    res = r.execute_strategy(spec.strategy_id, {
        'symbols': spec.symbols, 'start_date': start or spec.start, 'end_date': end or spec.end,
        'initial_cash': spec.cash, 'resolution': 'daily',
        'strategy_params': spec.params if params is None else params,
        'cost_model': spec.cost_model, 'fill_timing': spec.fill, 'on_bar': on_bar,
        'auto_warm_indicators': auto_warm})
    if res.get('success'):
        return res['data']
    msg = str(res.get('error') or 'The strategy failed in the Fincept engine.')
    if res.get('not_supported'):
        raise EngineRunError(msg, 'ENGINE', res.get('traceback'), True, res.get('feature'))
    exc = res.get('exception') or ''
    low = msg.lower()
    if exc == 'PriceDataError' or low.startswith('no historical data') or 'yahoo finance returned nothing' in low:
        raise EngineDataError(msg)
    if exc == 'ValueError' and (low.startswith('this strategy trades') or 'subscribes to no symbol' in low
                                or 'start date' in low or 'parameter' in low):
        raise EngineInputError(msg)
    raise EngineRunError(msg, 'ENGINE', res.get('traceback'))


# ════════════════════════════════════════════════════════════════════════════
# Fills -> trades (average cost, multipliers, carry, RAW-mode splits)
# ════════════════════════════════════════════════════════════════════════════

def _day(ts) -> str:
    return str(ts).split(' ')[0].split('T')[0]


_STOP_TYPES = {'STOP_MARKET': 'stop_loss', 'STOP_LIMIT': 'stop_loss', 'TRAILING_STOP': 'trailing_stop'}


def round_trips(fills: List[Dict[str, Any]], open_positions: Optional[List[Dict[str, Any]]] = None,
                carry_open: Optional[Dict[str, Dict[str, float]]] = None,
                corporate_events: Optional[List[Dict[str, Any]]] = None) -> List[Dict[str, Any]]:
    """Contract trades from the broker's fills (average-cost pairing per symbol, quantities in UNITS =
    contracts x multiplier, P&L net of commission, borrow fees and plus cash dividends of the position).
    Positions still open at the end are open trades (exitDate None, exitReason 'open', marked to market)."""
    pos: Dict[str, Dict[str, Any]] = {}
    rows: List[Dict[str, Any]] = []
    events = [(_day(f.get('time')), 0, i, f) for i, f in enumerate(fills or []) if isinstance(f, dict)]
    events += [(_day(e.get('time')), 1, i, e) for i, e in enumerate(corporate_events or [])]
    events.sort(key=lambda x: (x[0], x[1], x[2]))

    def new_pos():
        return {'qty': 0.0, 'avg': 0.0, 'opened': None, 'cpu': 0.0, 'spu': 0.0, 'borrow': 0.0, 'div': 0.0, 'm': 1.0}

    for day, kind, _i, f in events:
        sym = str(f.get('symbol', ''))
        p = pos.setdefault(sym, new_pos())
        if kind == 1:                       # split applied to a RAW-mode holding
            r = _num(f.get('split'), 0.0) or 0.0
            if r > 0 and p['qty']:
                p['qty'] *= r
                p['avg'] /= r
                p['cpu'] /= r
                p['spu'] /= r
            continue
        m = _num(f.get('multiplier'), 1.0) or 1.0
        q = (_num(f.get('quantity'), 0.0) or 0.0) * m
        px = _num(f.get('price'), 0.0) or 0.0
        fee = abs(_num(f.get('commission'), 0.0) or 0.0)
        slip = abs(_num(f.get('slippage'), 0.0) or 0.0)
        p['borrow'] += _num(f.get('borrow'), 0.0) or 0.0
        p['div'] += _num(f.get('dividends'), 0.0) or 0.0
        if q == 0 or px < 0:            # px 0: an option that expired worthless (or the option leg of an exercise)
            continue
        p['m'] = m
        fee_pu, slip_pu = fee / abs(q), slip / abs(q)
        qty = p['qty']
        if qty == 0 or (qty > 0) == (q > 0):
            new = qty + q
            p['avg'] = (abs(qty) * p['avg'] + abs(q) * px) / abs(new)
            p['cpu'] = (abs(qty) * p['cpu'] + abs(q) * fee_pu) / abs(new)
            p['spu'] = (abs(qty) * p['spu'] + abs(q) * slip_pu) / abs(new)
            if qty == 0:
                p['opened'] = day
            p['qty'] = new
            continue
        closing = min(abs(q), abs(qty))
        share = closing / abs(qty)
        direction = 1.0 if qty > 0 else -1.0
        commission = (p['cpu'] + fee_pu) * closing
        borrow = p['borrow'] * share
        div = p['div'] * share
        gross = (px - p['avg']) * closing * direction
        pnl = gross - commission - borrow + div
        typ = str(f.get('type') or '')
        tag = str(f.get('tag') or '')
        reason = 'margin_call' if tag.startswith('Margin call') else _STOP_TYPES.get(typ, 'signal')
        row = {'symbol': sym, 'side': 'long' if direction > 0 else 'short', 'entryDate': p['opened'],
               'entryPrice': p['avg'], 'exitDate': day, 'exitPrice': px, 'quantity': closing, 'pnl': pnl,
               'pnlPercent': (pnl / (p['avg'] * closing)) if p['avg'] > 0 else None, 'commission': commission,
               'slippageCost': (p['spu'] + slip_pu) * closing, 'borrowCost': borrow, 'dividends': div,
               'exitReason': reason, 'exitOrderType': typ or None, 'mae': None, 'mfe': None}
        if m != 1.0:
            row.update({'multiplier': m, 'contracts': closing / m})
        rows.append(row)
        new = qty + q
        p['borrow'] -= borrow
        p['div'] -= div
        if abs(new) < 1e-12:
            pos[sym] = new_pos()
        elif (new > 0) != (qty > 0):        # flipped: the remainder opens the other way at px
            pos[sym] = dict(new_pos(), qty=new, avg=px, opened=day, cpu=fee_pu, spu=slip_pu, m=m)
        else:                                # partial reduce keeps the cost basis
            p['qty'] = new

    for op in open_positions or []:
        sym = str(op.get('symbol', ''))
        p = pos.get(sym)
        mark = _num(op.get('market_price'), 0.0) or 0.0
        if p is None or not p['qty'] or mark <= 0:
            continue
        units = p['qty']
        c = (carry_open or {}).get(sym) or {}
        borrow = p['borrow'] + (_num(c.get('borrow'), 0.0) or 0.0)
        div = p['div'] + (_num(c.get('dividends'), 0.0) or 0.0)
        direction = 1.0 if units > 0 else -1.0
        commission = p['cpu'] * abs(units)
        pnl = (mark - p['avg']) * abs(units) * direction - commission - borrow + div
        row = {'symbol': sym, 'side': 'long' if direction > 0 else 'short', 'entryDate': p['opened'],
               'entryPrice': p['avg'], 'exitDate': None, 'exitPrice': None, 'markPrice': mark,
               'quantity': abs(units), 'pnl': pnl, 'pnlPercent': (pnl / (p['avg'] * abs(units))) if p['avg'] > 0
               else None, 'commission': commission, 'slippageCost': p['spu'] * abs(units), 'borrowCost': borrow,
               'dividends': div, 'exitReason': 'open', 'mae': None, 'mfe': None}
        if p['m'] != 1.0:
            row.update({'multiplier': p['m'], 'contracts': abs(units) / p['m']})
        rows.append(row)
    return rows


def fill_rows(fills: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    out = []
    for f in fills or []:
        q = _num(f.get('quantity'), 0.0) or 0.0
        row = {'date': _day(f.get('time')), 'symbol': f.get('symbol'), 'side': 'buy' if q > 0 else 'sell',
               'quantity': abs(q), 'price': _num(f.get('price')), 'commission': abs(_num(f.get('commission'), 0.0)
                                                                                  or 0.0),
               'slippageCost': abs(_num(f.get('slippage'), 0.0) or 0.0), 'rawPrice': _num(f.get('raw_price')),
               'orderType': f.get('type'), 'type': f.get('type'), 'orderId': f.get('order_id'),
               'tag': f.get('tag') or ''}
        for k in ('multiplier', 'borrow', 'dividends'):
            if f.get(k):
                row[k] = f[k]
        out.append(row)
    return out


def trade_extras(trades: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Robustness extras from closed trades: count, win rate, profit factor, SQN."""
    pnl = [float(t['pnl']) for t in trades if t.get('exitDate') and _num(t.get('pnl')) is not None]
    n = len(pnl)
    if not n:
        return {'trades': 0, 'winRate': None, 'profitFactor': None, 'sqn': None}
    wins = [x for x in pnl if x > 0]
    losses = [x for x in pnl if x < 0]
    mean = sum(pnl) / n
    sd = math.sqrt(sum((x - mean) ** 2 for x in pnl) / (n - 1)) if n > 1 else 0.0
    return {'trades': n, 'winRate': len(wins) / n, 'profitFactor': (sum(wins) / -sum(losses)) if losses else None,
            'sqn': (math.sqrt(n) * mean / sd) if sd > 0 else None}


# ════════════════════════════════════════════════════════════════════════════
# Result
# ════════════════════════════════════════════════════════════════════════════

def identity(spec: Spec, command: str = 'run_backtest', modelled_borrow: bool = True):
    from base.result_normalizer import EngineIdentity
    impl = IMPLEMENTATION
    if command in ('optimize', 'walk_forward'):
        impl = robust_implementation()
    modelled = ['margin calls']
    if modelled_borrow:
        modelled.append('borrow cost')
    return EngineIdentity(ENGINE, impl, library=None, version=ENGINE_VERSION, fill_mode=spec.fill,
                          commission_itemised=True, slippage_itemised=True, modelled=modelled, trade_mode='signals',
                          notes=[f'Executed by the Fincept bar-loop engine (strategies/fincept_engine) - its broker, '
                                 f'not base.simulator; fill timing {spec.fill}'
                                 + (' (the engine\'s native rule)' if spec.fill == 'next_open' else ' (requested)')
                                 + '.'])


def robust_implementation() -> str:
    return (f'{IMPLEMENTATION}; every combination is a full engine run of the strategy with its get_parameter values '
            f'set, scored by base.robustness')


def execution_text(spec: Spec) -> str:
    at = {'next_open': 'OPEN', 'next_close': 'CLOSE'}.get(spec.fill)
    if at:
        return NATIVE_FILL_TEXT.format(at=at)
    return (NATIVE_FILL_TEXT.format(at='OPEN') + ' fillTiming same_close (requested): MARKET orders placed once a bar '
            'is known (on_data, the framework, events at or after the close) fill at THAT bar\'s close instead - '
            'optimistic.')


def _frames_from_bars(bars: Dict[str, Any], start: str, end: str, modes: Dict[str, str]) -> Dict[str, Any]:
    import pandas as pd
    out = {}
    for sym, b in (bars or {}).items():
        try:
            idx = pd.DatetimeIndex(pd.to_datetime(b['dates']))
            df = pd.DataFrame({'Open': b['open'], 'High': b['high'], 'Low': b['low'], 'Close': b['close'],
                               'Volume': b['volume']}, index=idx).astype(float)
            out[sym] = df.loc[(df.index >= pd.Timestamp(start)) & (df.index <= pd.Timestamp(end))]
        except Exception as e:
            _log(f'OHLC frame for {sym} skipped: {e}')
    return out


def _symbol_provenance(spec: Spec, inner: Dict[str, Any], frames: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    """price_provenance for build_backtest_result: bars / firstBar / lastBar inside the run's window for every
    series the run priced (keyed by the strategy's symbol and by the user's spelling of it)."""
    from base.price_store import currency_for_symbol
    led = (inner.get('price_feed') or {}).get('provenance') or {}
    smap = inner.get('symbol_map') or {}
    modes = inner.get('price_modes') or {}
    out = {}
    for key, f in frames.items():
        y = smap.get(key, key)
        p = led.get(y) or {}
        rec = {'symbol': key, 'yahooTicker': y, 'bars': int(len(f)),
               'firstBar': f.index[0].strftime('%Y-%m-%d') if len(f) else None,
               'lastBar': f.index[-1].strftime('%Y-%m-%d') if len(f) else None,
               'currency': p.get('currency') or currency_for_symbol(y), 'source': p.get('source', 'yahoo'),
               'cacheAsOf': p.get('cacheAsOf'), 'fromCache': p.get('fromCache'), 'missingBars': None,
               'stale': p.get('stale', False), 'lastBarProvisional': p.get('lastBarProvisional', False),
               'firstAvailable': p.get('firstAvailable'), 'listingKnown': p.get('listingKnown'),
               'loadedFrom': p.get('loadedFrom'), 'loadedTo': p.get('loadedTo'),
               'adjustment': {'raw': 'none (as traded)', 'split_adjusted': 'split'}.get(modes.get(key, ''),
                                                                                       'split+dividend')}
        if not led.get(y):
            rec['source'] = 'derived' if y == key and key not in led else rec['source']
        out[key] = rec
    norm = lambda s: str(s or '').strip().upper().replace('/', '')
    for u in spec.symbols:
        for key, rec in list(out.items()):
            if norm(u) in (norm(key), norm(rec.get('yahooTicker'))) and u.upper() not in out:
                out[u.upper()] = dict(rec, symbol=u.upper())
    return out


def _substitutions(inner: Dict[str, Any]) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    """(substitutions [{requested, used, reason, kind}], warnings)."""
    subs, warns, spell = [], [], []
    for m in inner.get('symbol_mapping') or []:
        reason = m.get('reason')
        kind = 'substituted' if reason else 'spelling'
        subs.append({'requested': m.get('requested'), 'used': m.get('used'),
                     'reason': reason or 'Yahoo Finance spelling of the same instrument', 'kind': kind})
        if reason:
            warns.append({'code': 'TICKER_SUBSTITUTED', 'severity': 'info', 'symbol': m.get('requested'),
                          'message': f"{m.get('requested')} was priced from Yahoo {m.get('used')}: {reason}."})
        else:
            spell.append(f"{m.get('requested')} -> {m.get('used')}")
    if spell:
        warns.append({'code': 'TICKER_SPELLING', 'severity': 'info', 'symbol': None,
                      'message': 'Yahoo Finance tickers used for the strategy\'s symbols: ' + ', '.join(spell[:12])
                                 + ('...' if len(spell) > 12 else '') + '.'})
    return subs, warns


def _engine_warnings(inner: Dict[str, Any], spec: Spec, req: Dict[str, Any]) -> List[Dict[str, Any]]:
    w = []

    def add(code, msg, sev='warn', sym=None):
        w.append({'code': code, 'severity': sev, 'message': msg, 'symbol': sym})
    n_sub = int(inner.get('orders_submitted') or 0)
    n_fill = int(inner.get('orders_filled') or 0)
    if n_sub == 0:
        add('NO_ORDERS', 'The strategy placed NO orders in this period: the flat equity line is not a result (its '
                         'entry conditions never triggered, or it needs data this engine does not provide).')
    elif n_fill == 0:
        add('NO_FILLS', f'The strategy placed {n_sub} order(s) but none filled: the equity line is not a result.')
    if inner.get('orders_rejected'):
        reasons = '; '.join(f'{k} ({v})' for k, v in (inner.get('rejection_reasons') or {}).items())
        add('ORDERS_REJECTED', f"{inner['orders_rejected']} order(s) rejected: {reasons}.", 'info')
    if inner.get('unsimulated_order_types'):
        add('ORDER_TYPE_NOT_SIMULATED', 'Order types not simulated (they never fill): '
            + ', '.join(inner['unsimulated_order_types']) + '.')
    if inner.get('margin_calls'):
        add('MARGIN_CALLS', f"{inner['margin_calls']} margin call(s): positions were cut at the next open because "
                            f"margin used exceeded equity.", 'info')
    if inner.get('uses_model_priced_derivatives'):
        add('MODEL_PRICED_DATA', 'Option prices in this run are MODEL-DERIVED (Black-Scholes / Black-76 on the real '
                                 'underlying bars), not historical option quotes.')
    if inner.get('uses_derived_futures_contracts'):
        add('DERIVED_FUTURES', 'Individual futures contracts are DERIVED from Yahoo\'s continuous front-month series '
                               '(cost of carry), not historical contract prices.')
    if inner.get('uses_universe_selection'):
        add('SURVIVORSHIP_BIAS', 'Universe selection draws from a fixed pool of today\'s large US stocks / ETFs '
                                 '(survivorship bias' + (', current fundamentals' if inner.get('uses_fundamental_snapshot')
                                                         else '') + ').', 'info')
    errs = (inner.get('price_feed') or {}).get('errors') or {}
    if errs and inner.get('uses_universe_selection'):
        names = ', '.join(list(errs)[:8]) + ('...' if len(errs) > 8 else '')
        add('SYMBOL_DROPPED', f'{len(errs)} symbol(s) the run asked for (candidate pool / universe members) have no '
                              f'price data in the period and were left out: {names}.', 'info')
    else:
        for t, msg in list(errs.items())[:10]:
            add('SYMBOL_DROPPED', f'No price data for {t}: {msg}; the strategy ran without it.', 'warn', t)
    return w


def _weights(inner: Dict[str, Any], dates: List[str], equity) -> Optional[Dict[str, Any]]:
    import numpy as np
    from base.execution import weights_block
    pv = inner.get('position_values') or []
    if len(pv) != len(dates):
        return None
    peak: Dict[str, float] = {}
    for snap in pv:
        for k, v in snap.items():
            peak[k] = max(peak.get(k, 0.0), abs(v))
    if len(peak) < 2:
        return None
    syms = sorted(peak, key=lambda k: -peak[k])[:40]
    vals = np.zeros((len(dates), len(syms)))
    col = {s: j for j, s in enumerate(syms)}
    for i, snap in enumerate(pv):
        for k, v in snap.items():
            j = col.get(k)
            if j is not None:
                vals[i, j] = v
    return weights_block(dates, syms, vals, np.asarray(equity, dtype=float))


def _costs_from_fills(fills, dates, equity, inner) -> Dict[str, Any]:
    import pandas as pd
    eq = pd.Series([float(x) for x in equity], index=pd.DatetimeIndex(pd.to_datetime(dates)))
    by: Dict[int, Dict[str, float]] = {}
    notional = 0.0
    for f in fills or []:
        m = _num(f.get('multiplier'), 1.0) or 1.0
        v = abs((_num(f.get('quantity'), 0.0) or 0.0) * (_num(f.get('price'), 0.0) or 0.0) * m)
        notional += v
        y = int(_day(f.get('time'))[:4])
        row = by.setdefault(y, {'year': y, 'commission': 0.0, 'slippage': 0.0, 'tradedNotional': 0.0})
        row['commission'] += abs(_num(f.get('commission'), 0.0) or 0.0)
        row['slippage'] += abs(_num(f.get('slippage'), 0.0) or 0.0)
        row['tradedNotional'] += v
    for y, row in by.items():
        ey = eq[eq.index.year == y]
        row['turnover'] = (row['tradedNotional'] / float(ey.mean())) if len(ey) and ey.mean() > 0 else None
    return {'totalCommission': float(inner.get('total_fees') or 0.0),
            'totalSlippage': float(inner.get('total_slippage') or 0.0),
            'totalBorrow': float(inner.get('total_borrow') or 0.0),
            'tradedNotional': notional, 'fills': len(fills or []), 'byYear': [by[y] for y in sorted(by)],
            'sensitivityNote': SENSITIVITY_NOTE}


def _assumptions(spec: Spec, inner: Dict[str, Any], ignored: List[str]) -> Dict[str, Any]:
    notes = []
    n_sub = int(inner.get('orders_submitted') or 0)
    n_fill = int(inner.get('orders_filled') or 0)
    if inner.get('unfilled_orders'):
        notes.append(f"{inner['unfilled_orders']} order(s) still open at the end "
                     f"({', '.join(inner.get('unfilled_order_types') or [])}).")
    if inner.get('warmup_orders_ignored'):
        notes.append(f"{inner['warmup_orders_ignored']} order(s) placed during warm-up were ignored.")
    notes.extend(inner.get('notes') or [])
    for d in inner.get('model_priced_disclosures') or []:
        if d not in notes:
            notes.append(d)
    notes.append(SENSITIVITY_NOTE)
    data_source = ('Yahoo Finance daily bars through the Fincept price cache (base.price_store; split- and dividend-'
                   'adjusted unless the strategy subscribes RAW / SPLIT_ADJUSTED prices)')
    smap = inner.get('symbol_map') or {}
    mapped = [f'{k} -> {v}' for k, v in smap.items() if k != v]
    if mapped:
        data_source += '; Yahoo tickers used: ' + ', '.join(mapped[:12])
    if inner.get('uses_model_priced_derivatives'):
        data_source += ('; OPTION PRICES ARE MODEL-DERIVED (Black-Scholes/Black-76 on the real underlying bars), '
                        'not historical option quotes')
    if inner.get('uses_derived_futures_contracts'):
        data_source += ('; individual futures contracts are DERIVED from Yahoo\'s continuous front-month series '
                        '(cost of carry), not historical contract prices')
    if inner.get('uses_universe_selection'):
        data_source += ('; universe selection from a fixed pool of today\'s large US stocks/ETFs (survivorship bias'
                        + (', current fundamentals' if inner.get('uses_fundamental_snapshot') else '') + ')')
    if inner.get('uses_custom_data'):
        data_source += '; custom data files read with the strategy\'s own reader: ' + \
            ', '.join((inner.get('custom_data_sources') or [])[:3])
    cm = spec.cost_model
    costs = cm.describe() + ('; fees / slippage of securities whose strategy sets its own fee or slippage model come '
                             'from that model')
    if cm.borrow_rate_annual:
        costs += f'; short borrow {cm.borrow_rate_annual * 100:.2f}%/yr on equity shorts (ACT/{int(cm.day_count)})'
    a = {
        'engine': IMPLEMENTATION,
        'execution': execution_text(spec),
        'costs': costs,
        'dataSource': data_source,
        'survivorship': ('Backtests the symbols the strategy trades (its own code decides them); delisted symbols '
                         'cannot be tested.'),
        'positionSizing': ('decided by the strategy code (set_holdings / order quantities: set_holdings sizes on '
                           'the last close with a 0.25% cash buffer, grossed up by the commission and slippage '
                           'rates); orders that would push margin above equity are rejected (default leverage: '
                           'equities 2x, forex 50x, crypto / futures / indices 1x; strategy leverage capped per '
                           'asset class)'),
        'ignoredSettings': list(ignored),
        'notes': notes,
        'decidedBy': 'the strategy\'s own code (QCAlgorithm API)',
        'executedBy': 'fincept_engine broker (not base.simulator)',
        'fillTiming': spec.fill,
        'orders': {'submitted': n_sub, 'filled': n_fill, 'rejected': int(inner.get('orders_rejected') or 0)},
    }
    if inner.get('warmup_bars'):
        a['warmUp'] = f"{inner['warmup_bars']} bars before the start date (orders ignored)"
    return a


def build_result(spec: Spec, req: Dict[str, Any], inner: Dict[str, Any], t0: float) -> Dict[str, Any]:
    from base.result_normalizer import build_backtest_result
    eq_rows = inner.get('equity') or []
    dates = [_day(p.get('time')) for p in eq_rows]
    equity = [float(p.get('equity')) for p in eq_rows]
    fills = inner.get('trades') or []
    anchored = False
    prev = inner.get('previous_session')
    if equity and prev and prev < dates[0] and abs(equity[0] - spec.cash) > 1e-9 * spec.cash and fills and \
            _day(fills[0].get('time')) == dates[0]:
        # orders placed in initialize() (or by a morning event) filled at the FIRST bar's open: the starting
        # capital stands at the previous session's close, so the first bar's P&L counts in every figure
        dates.insert(0, prev)
        equity.insert(0, float(spec.cash))
        anchored = True
    trades = round_trips(fills, inner.get('open_positions'), inner.get('carry_open'), inner.get('corporate_events'))
    for i, t in enumerate(trades, 1):
        t['id'] = f'trade_{i}'
    first, last = (dates[0], dates[-1]) if dates else (spec.start, spec.end)
    modes = inner.get('price_modes') or {}
    frames = _frames_from_bars(inner.get('bars'), first, last, modes)
    prov = _symbol_provenance(spec, inner, frames)
    # derived instruments the strategy traded (model-priced option / futures contracts) over the days held:
    # every trade's MAE / MFE and exposure from its OWN prices
    for k, f in _frames_from_bars(inner.get('traded_bars'), first, last, modes).items():
        frames.setdefault(k, f)
    bench = str(req.get('benchmarkSymbol') or '').strip().upper()
    if bench and modes.get(bench) in ('raw', 'split_adjusted'):
        frames.pop(bench, None)             # the benchmark must be total-return (adjusted), not raw prices
    ignored = list(spec.ignored)
    shorted = any((_num(f.get('quantity'), 0.0) or 0.0) < 0 for f in fills) and any(t['side'] == 'short'
                                                                                     for t in trades)
    if shorted and req.get('allowShort') is False:
        ignored.append('allowShort false (the strategy code decides; it sold short)')
    subs, sub_warn = _substitutions(inner)
    warnings = sub_warn + _engine_warnings(inner, spec, req)
    extra = {
        'usesModelPricedDerivatives': bool(inner.get('uses_model_priced_derivatives')),
        'usesDerivedFuturesContracts': bool(inner.get('uses_derived_futures_contracts')),
        'usesUniverseSelection': bool(inner.get('uses_universe_selection')),
        'usesFundamentalSnapshot': bool(inner.get('uses_fundamental_snapshot')),
        'usesCustomData': bool(inner.get('uses_custom_data')),
        'modelPricedDisclosures': list(inner.get('model_priced_disclosures') or []),
        'strategy': {'id': spec.strategy_id, 'name': spec.strategy_name, 'parameters': dict(spec.params)},
    }
    pv = ([{}] if anchored else []) + list(inner.get('position_values') or [])
    wb = _weights(dict(inner, position_values=pv), dates, equity)
    if wb:
        extra['weights'] = wb
    if inner.get('runtime_statistics'):
        extra['runtimeStatistics'] = dict(inner['runtime_statistics'])
    logs = [f'Fincept Engine backtest of {spec.strategy_name} ({spec.strategy_id})',
            f"Symbols: {', '.join(inner.get('symbols') or spec.symbols)}",
            f'Period: {first} to {last} ({len(dates)} bars)',
            f"Orders: {int(inner.get('orders_submitted') or 0)} placed, {int(inner.get('orders_filled') or 0)} "
            f"filled, {int(inner.get('orders_rejected') or 0)} rejected",
            f"Price store: {(inner.get('price_feed') or {}).get('network_calls', 0)} download(s)"]
    if anchored:
        logs.append(f'The strategy\'s first orders filled at the open of {dates[1]}: the equity curve starts with '
                    f'the starting capital at the previous session\'s close ({dates[0]}).')
    data = build_backtest_result(
        req, dates=dates, equity=equity, identity=identity(spec, modelled_borrow=bool(spec.cost_model.borrow_rate_annual)),
        trades=trades, fills=fill_rows(fills), costs=_costs_from_fills(fills, dates, equity, inner),
        cost_model=spec.cost_model, frames=frames, price_provenance=prov,
        assumptions=_assumptions(spec, inner, ignored), warnings=warnings, logs=logs,
        warmup_bars=int(inner.get('warmup_bars') or 0), using_synthetic=False, extra=extra)
    st = data.get('statistics') or {}
    st.update({'ordersSubmitted': int(inner.get('orders_submitted') or 0),
               'ordersFilled': int(inner.get('orders_filled') or 0),
               'ordersRejected': int(inner.get('orders_rejected') or 0),
               'totalBorrow': float(inner.get('total_borrow') or 0.0)})
    data['statistics'] = st
    pv = data.get('provenance') or {}
    pf = inner.get('price_feed') or {}
    seen = set()
    series = []
    for rec in prov.values():
        if rec['symbol'] in seen or rec['symbol'] not in frames:
            continue
        seen.add(rec['symbol'])
        series.append(rec)
    pv.update({'symbolMap': dict(inner.get('symbol_map') or {}), 'substitutions': subs,
               'priceSeries': series, 'executedBy': 'fincept_engine', 'decidedBy': 'strategy code',
               'fillTiming': spec.fill, 'priceStoreDownloads': int(pf.get('network_calls') or 0),
               'securityTypes': dict(inner.get('security_types') or {}),
               'engineRunMs': int((time.perf_counter() - t0) * 1000)})
    data['provenance'] = pv
    perf = data.get('performance') or {}
    perf['totalOrders'] = int(inner.get('orders_filled') or 0)      # v1: fills (buys + sells)
    data['performance'] = perf
    if isinstance(data.get('costs'), dict):
        data['costs']['sensitivityNote'] = SENSITIVITY_NOTE
    return data


# ════════════════════════════════════════════════════════════════════════════
# Commands
# ════════════════════════════════════════════════════════════════════════════

def _run_progress(req: Dict[str, Any]):
    """on_bar for run_backtest: 'run' progress lines (rate-limited) + the host's cancel flag."""
    from base.progress import CancelToken, Progress
    token = CancelToken(req)
    state = {'p': None, 'w': None}

    def on_bar(i, n, day, warming):
        token.check()
        if warming:
            if state['w'] is None:
                state['w'] = Progress('run', total=n, message=f'Warm-up: {n} bars')
            state['w'].update(i + 1, f'Warm-up {i + 1} / {n}')
            return
        if state['p'] is None:
            state['p'] = Progress('run', total=n, message=f'Running {n} bars')
        state['p'].update(i + 1, f'Bar {i + 1} / {n} ({day:%Y-%m-%d})')
    return on_bar, state


def run_backtest(req: Dict[str, Any]) -> Dict[str, Any]:
    t0 = time.perf_counter()
    spec = parse_request(req)
    on_bar, st = _run_progress(req)
    inner = run_engine(spec, on_bar=on_bar)
    if st['p'] is not None:
        st['p'].finish('Run done')
    return {'success': True, 'data': build_result(spec, req, inner, t0)}


# ── optimise / walk-forward ─────────────────────────────────────────────────

def _param_ranges(req: Dict[str, Any], spec: Spec) -> Dict[str, Any]:
    import fe_catalog as CAT
    known = {p['id']: p for p in CAT.strategy_parameters(runner(), spec.strategy_id)}
    numeric = {k: p for k, p in known.items() if p.get('type') in ('int', 'float')}
    if not numeric:
        raise EngineInputError(f'{spec.strategy_name} cannot be optimised: {CAT.NOT_OPTIMIZABLE}.')
    ranges = req.get('paramRanges') or req.get('parameters')
    if not isinstance(ranges, dict) or not ranges:
        raise EngineInputError('No parameter ranges supplied (paramRanges with min / max / step per parameter; '
                               f'{spec.strategy_name} reads {", ".join(numeric)}).')
    unknown = [k for k in ranges if k not in known]
    if unknown:
        raise EngineInputError(f'{", ".join(unknown)} {"is not a parameter" if len(unknown) == 1 else "are not parameters"}'
                               f' {spec.strategy_name} reads (it reads: {", ".join(known)}).')
    out = {}
    for k, v in ranges.items():
        if isinstance(v, dict) and known[k].get('type') == 'float':
            v = dict(v, type='float')
        out[k] = v
    return out


def _combos(req: Dict[str, Any], spec: Spec, default_max: int):
    from base import robustness as R
    ranges = _param_ranges(req, spec)
    method = str(req.get('optimizeMethod') or req.get('method') or 'grid')
    max_it = req.get('maxIterations') or default_max
    seed = req.get('seed', req.get('optimizeSeed'))
    seed = int(_num(seed, 7)) if seed is not None else 7
    try:
        combos, info = R.sample_combinations(ranges, method=method, max_iterations=max_it, seed=seed)
    except ValueError as e:
        raise EngineInputError(str(e))
    import fe_catalog as CAT
    warnings = []
    cons = CAT.constraints_for(CAT.strategy_parameters(runner(), spec.strategy_id))
    if cons:
        base = dict(spec.params)

        def ok(c):
            p = dict(base, **c)
            return all(not (isinstance(p.get(x['lt'][0]), (int, float)) and isinstance(p.get(x['lt'][1]), (int, float))
                            and p[x['lt'][0]] >= p[x['lt'][1]]) for x in cons)
        total = int(info.get('totalCombinations') or len(combos))
        if total <= 200_000:
            # sample among the VALID combinations, so maxIterations combinations are really evaluated
            import numpy as np
            full, _ = R.sample_combinations(ranges, method='grid', max_iterations=total, seed=seed)
            valid = [c for c in full if ok(c)]
            if not valid:
                raise EngineInputError(f'Every one of the {total} combinations breaks a rule of '
                                       f'{spec.strategy_name} ({cons[0]["message"]}).')
            cap = int(float(max_it)) if max_it not in (None, '', 0) else len(valid)
            if method.strip().lower() == 'random' or len(valid) > cap:
                k = min(cap, len(valid))
                idx = sorted(np.random.default_rng(seed).choice(len(valid), size=k, replace=False).tolist())
                combos = [valid[i] for i in idx]
            else:
                combos = valid
            dropped = total - len(valid)
            info = dict(info, iterations=len(combos), validCombinations=len(valid), skippedByConstraints=dropped,
                        truncated=len(combos) < len(valid),
                        sampling=('full grid of the valid combinations' if len(combos) == len(valid) else
                                  f'seeded sample without replacement ({len(combos)} of {len(valid)} valid '
                                  f'combinations, {total} in the grid)'))
        else:
            keep = [c for c in combos if ok(c)]
            if not keep:
                raise EngineInputError(f'Every one of the {len(combos)} combinations breaks a rule of '
                                       f'{spec.strategy_name} ({cons[0]["message"]}).')
            dropped = len(combos) - len(keep)
            combos = keep
            info = dict(info, iterations=len(combos), skippedByConstraints=dropped)
        if dropped:
            warnings.append({'code': 'COMBINATIONS_SKIPPED', 'severity': 'info', 'symbol': None,
                             'message': f'{dropped} combinations of the grid break a rule of {spec.strategy_name} '
                                        f'({cons[0]["message"]}) and are not evaluated.'})
    return combos, info, seed, warnings


def _workers(req: Dict[str, Any], trials: int, per_trial_s: float) -> int:
    """A spawn worker costs ~2 s on Windows (imports + the engine); a pool only pays above ~4 s of work."""
    from base import robustness as R
    if req.get('maxWorkers'):
        return R.max_workers(req, trials)
    if trials * per_trial_s < 4.0:
        return 1
    return R.max_workers(req, trials)


def _setup_robust(req: Dict[str, Any], command: str, default_max: int):
    from base import robustness as R
    spec = parse_request(req)
    combos, info, seed, warns = _combos(req, spec, default_max)
    try:
        objective = R.normalize_objective(req.get('optimizeObjective') or req.get('objective') or 'sharpe')
    except ValueError as e:
        raise EngineInputError(str(e))
    # Reference run (the base parameters over the whole period): the bar axis of every window, the price
    # memo warmed once, a plain error before hundreds of failing trials, and the per-trial time.
    from base.progress import emit
    emit(command, 0, None, 'Reference run of the strategy', force=True)
    on_bar, pst = _run_progress(req)
    t = time.perf_counter()
    ref = run_engine(spec, on_bar=on_bar)
    ref_s = time.perf_counter() - t
    if pst['p'] is not None:
        pst['p'].finish('Reference run done')
    dates = [_day(p.get('time')) for p in ref.get('equity') or []]
    if len(dates) < 30:
        raise EngineDataError(f'The strategy produced only {len(dates)} bars between {spec.start} and {spec.end}; '
                              f'too few to optimise.')
    return spec, combos, info, seed, warns, objective, ref, ref_s, dates


def _robust_meta(out: Dict[str, Any], spec: Spec, req: Dict[str, Any], command: str, warns, ref, dates,
                 t0: float, rf: float) -> Dict[str, Any]:
    import platform
    from base.execution import fill_model_block
    impl = robust_implementation()
    out['engine'] = ENGINE
    out['engineImplementation'] = impl
    out['engineVersion'] = ENGINE_VERSION
    out['usingSyntheticData'] = False
    out['strategy'] = spec.strategy_id
    out['strategyName'] = spec.strategy_name
    out['symbols'] = list(ref.get('symbols') or spec.symbols)
    subs, sub_warn = _substitutions(ref)
    out['warnings'] = list(warns) + list(out.get('warnings') or []) + sub_warn
    for s in spec.ignored:
        out['warnings'].append({'code': 'IGNORED_SETTING', 'severity': 'warn', 'symbol': None,
                                'message': f'Setting not applied by this engine: {s}.'})
    out['assumptions'] = {
        'engine': impl, 'execution': execution_text(spec), 'fillModel': fill_model_block(spec.fill),
        'costs': spec.cost_model.describe(), 'positionSizing': 'decided by the strategy code',
        'riskFreeRate': rf, 'riskFreeSource': 'request',
        'dataSource': 'Yahoo Finance via the Fincept price cache (base.price_store)',
        'objectiveNote': f'Objectives use the request\'s risk-free rate ({rf * 100:.2f}% a year) as a constant; '
                         f'Sharpe = mean excess bar return / std x sqrt(bars per year) (base.metrics).',
        'windowWarmUp': ('each window run (walk-forward) seeds the strategy\'s registered indicators with the bars '
                         'before the window (automatic indicator warm-up) unless the strategy sets its own warm-up'),
        'ignoredSettings': list(spec.ignored),
        'notes': [f'Every combination runs the strategy\'s own code in the Fincept engine with get_parameter values '
                  f'set; the full-period runs start {dates[0]} like run_backtest.']}
    pf = ref.get('price_feed') or {}
    out['provenance'] = {
        'engine': ENGINE, 'engineImplementation': impl, 'engineVersion': ENGINE_VERSION, 'library': None,
        'libraryVersion': None, 'pythonVersion': platform.python_version(), 'command': command,
        'dataSource': 'yahoo', 'interval': '1d', 'bars': len(dates), 'firstBar': dates[0], 'lastBar': dates[-1],
        'requestedStart': spec.start, 'requestedEnd': spec.end, 'symbols': list(ref.get('symbols') or spec.symbols),
        'symbolMap': dict(ref.get('symbol_map') or {}), 'substitutions': subs,
        'strategy': {'type': spec.strategy_id, 'name': spec.strategy_name}, 'baseParameters': dict(spec.params),
        'seed': out.get('seed'), 'executedBy': 'fincept_engine', 'fillTiming': spec.fill,
        'priceStoreDownloads': int(pf.get('network_calls') or 0),
        'durationMs': int((time.perf_counter() - t0) * 1000), 'schemaVersion': 2}
    out.setdefault('logs', []).append(
        f'{len(dates)} bars; {out.get("iterations") or out.get("combinationsPerWindow")} combinations; workers '
        f'{out.get("workers")}; {out["provenance"]["durationMs"]} ms.')
    return out


def _context(spec: Spec, dates: List[str], req: Dict[str, Any]) -> Dict[str, Any]:
    import uuid
    return {'token': uuid.uuid4().hex, 'strategy_id': spec.strategy_id, 'symbols': list(spec.symbols),
            'start': spec.start, 'end': spec.end, 'cash': spec.cash, 'base_params': dict(spec.params),
            'cost_model': asdict(spec.cost_model), 'fill': spec.fill, 'dates': list(dates),
            'cancel_file': req.get('cancelFile')}


def optimize(req: Dict[str, Any]) -> Dict[str, Any]:
    import fincept_evaluate as FE
    from base import robustness as R
    from base.metrics import infer_periods_per_year, to_datetime_index
    from base.progress import CancelToken
    t0 = time.perf_counter()
    spec, combos, info, seed, warns, objective, ref, ref_s, dates = _setup_robust(req, 'optimize', 500)
    idx = to_datetime_index(dates)
    ctx = _context(spec, dates, req)
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    hold = _num(req.get('holdoutFraction'), 0.0) or 0.0
    try:
        out = R.run_optimization(FE.evaluate, combos, context=ctx, objective=objective, ppy=infer_periods_per_year(idx),
                                 dates=idx, rf=rf, info=info, request=req, seed=seed, holdout_fraction=hold,
                                 cancel=CancelToken(req), workers=_workers(req, len(combos), ref_s))
    except ValueError as e:
        raise EngineRunError(str(e))
    out['seed'] = seed
    out['referenceRunSec'] = round(ref_s, 3)
    return {'success': True, 'data': _robust_meta(out, spec, req, 'optimize', warns, ref, dates, t0, rf)}


def walk_forward(req: Dict[str, Any]) -> Dict[str, Any]:
    import fincept_evaluate as FE
    from base import robustness as R
    from base.metrics import infer_periods_per_year, to_datetime_index
    from base.progress import CancelToken
    t0 = time.perf_counter()
    spec, combos, info, seed, warns, objective, ref, ref_s, dates = _setup_robust(req, 'walk_forward', 200)
    k_win = int(_num(req.get('wfSplits') or req.get('nSplits'), 5) or 5)
    ratio = float(_num(req.get('wfTrainRatio') or req.get('trainRatio'), 0.7) or 0.7)
    mode = str(req.get('wfMode') or req.get('walkForwardMode') or 'rolling').lower()
    purge = int(_num(req.get('purgeBars'), 0) or 0)
    embargo = int(_num(req.get('embargoBars'), 0) or 0)
    try:
        windows = R.walk_forward_windows(len(dates), k_win, ratio, mode, purge_bars=purge, embargo_bars=embargo,
                                         warmup_bars=0)
    except ValueError as e:
        raise EngineInputError(str(e))
    idx = to_datetime_index(dates)
    ctx = _context(spec, dates, req)
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    trials = len(combos) * (len(windows) + 1)
    try:
        out = R.run_walk_forward(FE.evaluate, combos, windows, context=ctx, objective=objective,
                                 ppy=infer_periods_per_year(idx), dates=idx, rf=rf, initial_capital=spec.cash,
                                 mode=mode, purge_bars=purge, embargo_bars=embargo, request=req, seed=seed,
                                 cancel=CancelToken(req), workers=_workers(req, trials, ref_s * 0.6))
    except ValueError as e:
        raise EngineRunError(str(e))
    out['seed'] = seed
    out['warmupBars'] = 0
    out['warmUp'] = ('the engine seeds the strategy\'s indicators with the bars before each window (automatic '
                     'indicator warm-up from the history every run loads); no bars of the window are spent on it')
    spans = [(w.get('testStart'), w.get('testEnd'), w.get('window')) for w in out.get('windows') or []]
    for row in out.get('oosEquity') or []:
        for a, b, wi in spans:
            if a and b and a <= row['date'] <= b:
                row['window'] = wi
                break
    out['combinationsTotal'] = info.get('totalCombinations')
    out['sampling'] = info.get('sampling')
    return {'success': True, 'data': _robust_meta(out, spec, req, 'walk_forward', warns, ref, dates, t0, rf)}
