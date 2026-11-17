# ============================================================================
# Fincept Terminal - Strategy Engine Backtest Broker
# Order book, fills against daily bars, costs, buying power and margin calls.
#
# Fill rule (no look-ahead): an order placed while the strategy looks at bar t
# (prices up to t's close) can only fill on a LATER bar:
#   MARKET / MARKET_ON_OPEN  -> next bar's open (+/- slippage)
#   MARKET_ON_CLOSE          -> next bar's close (+/- slippage)
#   LIMIT                    -> next bar(s) whose range reaches the limit: at the limit,
#                               or at the open when the bar gaps through it
#   STOP_MARKET              -> next bar(s) whose range reaches the stop: at the stop,
#                               or at the open when it gaps through (+/- slippage)
#   STOP_LIMIT               -> stop triggers as above; fills only when the open already
#                               satisfies both prices, otherwise rests as a limit order
#   TRAILING_STOP            -> a stop that follows the price: checked against each bar with the
#                               stop as of the previous bar (gap at the open fills at the open),
#                               then moved with the bar's high (sell) / low (buy)
#   LIMIT_IF_TOUCHED         -> once a bar touches the trigger it becomes a limit order; fills on
#                               the touching bar only when its open already satisfies both prices
# Order events follow LEAN: Submitted when accepted, UpdateSubmitted for each accepted update,
# Filled / Canceled / Invalid; every request (submit, update, cancel) gets an OrderResponse.
# ============================================================================

import math
from datetime import datetime, timedelta
from typing import Dict, List, Optional

from .enums import OrderStatus, OrderType, SecurityType, OrderRequestStatus
from .types import OrderEvent
from .orders import OrderResponse, OrderResponseErrorCode as _EC

# Default leverage when the strategy did not set one, and the most the engine
# grants whatever the strategy asks for (overnight positions: daily bars).
_DEFAULT_LEVERAGE = {
    SecurityType.EQUITY: 2.0,       # Reg-T margin account
    SecurityType.FOREX: 50.0,
    SecurityType.CFD: 10.0,
    SecurityType.CRYPTO: 1.0,       # spot, cash account
    SecurityType.CRYPTO_FUTURE: 1.0,
    SecurityType.FUTURE: 1.0,       # fully funded: no contract multiplier is modelled
    SecurityType.INDEX: 1.0,
}
_MAX_LEVERAGE = {
    SecurityType.EQUITY: 2.0,
    SecurityType.FOREX: 50.0,
    SecurityType.CFD: 20.0,
    SecurityType.CRYPTO: 1.0,
    SecurityType.CRYPTO_FUTURE: 20.0,
    SecurityType.FUTURE: 20.0,
    SecurityType.INDEX: 1.0,
}

_SIMULATED = {OrderType.MARKET, OrderType.MARKET_ON_OPEN, OrderType.MARKET_ON_CLOSE,
              OrderType.LIMIT, OrderType.STOP_MARKET, OrderType.STOP_LIMIT,
              OrderType.TRAILING_STOP, OrderType.LIMIT_IF_TOUCHED,
              OrderType.COMBO_MARKET, OrderType.COMBO_LIMIT, OrderType.COMBO_LEG_LIMIT}
_OPEN = (OrderStatus.SUBMITTED, OrderStatus.UPDATE_SUBMITTED, OrderStatus.PARTIALLY_FILLED)
_IMMEDIATE = {OrderType.MARKET, OrderType.MARKET_ON_OPEN, OrderType.MARKET_ON_CLOSE, OrderType.COMBO_MARKET}
_COMBO = {OrderType.COMBO_MARKET, OrderType.COMBO_LIMIT, OrderType.COMBO_LEG_LIMIT}


class _OrderFee:
    """LEAN-shaped fee: order_event.order_fee.value.amount / .currency."""
    def __init__(self, amount: float, currency: str = "USD"):
        self.value = type('CashAmount', (), {'amount': amount, 'currency': currency})()

    def __repr__(self):
        return f"OrderFee({self.value.amount:.4f})"


def lot_size(security_type) -> float:
    if security_type in (SecurityType.CRYPTO, SecurityType.CRYPTO_FUTURE):
        return 1e-8
    return 1.0


def round_to_lot(quantity: float, lot: float) -> float:
    """Round toward zero to a multiple of the lot size."""
    if lot >= 1:
        return float(int(quantity))
    steps = math.floor(abs(quantity) / lot + 1e-9)
    q = round(steps * lot, 8)
    return q if quantity >= 0 else -q


FILL_TIMINGS = ('next_open', 'next_close', 'same_close')


class BacktestBroker:
    """Order book and fills of one run.

    Costs: `commission_rate` (fraction of notional) and `slippage_rate` (adverse fraction of the price)
    - or `cost_model`, any object with base.costs.CostModel's fields (commission_type pct_notional |
    per_share | per_trade with min_commission / max_commission_pct, slippage_type pct_price |
    vol_scaled | none, spread_bps (half paid per fill), borrow_rate_annual, day_count). A fee /
    slippage / fill model the STRATEGY set on a security still wins for that security.
    Fill timing of MARKET orders (and combo legs): 'next_open' (the engine's native rule: the next
    bar's open), 'next_close' (the next bar's close), 'same_close' (orders placed once bar t is known
    fill at bar t's close - optimistic; requested explicitly). Other order types are unaffected.
    """

    def __init__(self, algorithm, commission_rate: float = 0.0, slippage_rate: float = 0.0, cost_model=None,
                 fill_timing: str = 'next_open'):
        self.algo = algorithm
        self.cost_model = cost_model
        if cost_model is not None:
            # the pct components also drive order sizing estimates (set_holdings grosses up by them)
            commission_rate = cost_model.commission_value if cost_model.commission_type == 'pct_notional' else 0.0
            slippage_rate = (cost_model.slippage_value if cost_model.slippage_type == 'pct_price' else 0.0) \
                + float(cost_model.spread_bps or 0.0) / 2e4
        self.commission_rate = max(0.0, float(commission_rate or 0.0))
        self.slippage_rate = max(0.0, float(slippage_rate or 0.0))
        self.fill_timing = fill_timing if fill_timing in FILL_TIMINGS else 'next_open'
        self.borrow_rate = max(0.0, float(getattr(cost_model, 'borrow_rate_annual', 0.0) or 0.0))
        self.day_count = float(getattr(cost_model, 'day_count', 360.0) or 360.0)
        self.total_borrow = 0.0
        self.carry: Dict[str, Dict[str, float]] = {}   # ticker -> money accrued since its last fill
        self.corporate_events: List[Dict] = []          # RAW-mode splits applied to holdings
        self._last_accrual = None
        self._vol_cache: Dict[tuple, float] = {}
        self._day = None
        self.book: List = []              # open tickets, submission order
        self.fills: List[Dict] = []       # one row per fill
        self.total_fees = 0.0
        self.total_slippage = 0.0         # money lost to slippage vs the raw price
        self.rejected: List[tuple] = []   # (ticket, reason)
        self.ignored_warmup = 0
        self.margin_calls = 0
        self.leverage_capped: Dict[str, tuple] = {}
        self.unsimulated_types = set()
        self.model_notes = set()          # securities whose fees/fills/slippage came from strategy models
        self.prechecks = []               # requests refused by pre-order checks (never became orders)
        # Derivative position groups (options with their underlying) add their own margin rule:
        # fn(positions {ticker: qty}, price(ticker)) -> (requirement, set of tickers it covered)
        self.group_margin = None

    # ---- leverage / margin -------------------------------------------------

    def leverage_of(self, ticker: str) -> float:
        sec = self.algo.securities.get(ticker)
        st = getattr(getattr(sec, 'symbol', None), 'security_type', SecurityType.EQUITY) if sec else SecurityType.EQUITY
        requested = getattr(sec, 'leverage', None) if sec else None
        default = _DEFAULT_LEVERAGE.get(st, 1.0)
        cap = _MAX_LEVERAGE.get(st, 1.0)
        try:
            lev = float(requested) if requested not in (None, 0) else default
        except (TypeError, ValueError):
            lev = default
        if lev > cap:
            self.leverage_capped[ticker] = (lev, cap)
            lev = cap
        return max(lev, 1e-9)

    def _price(self, ticker: str) -> float:
        sec = self.algo.securities.get(ticker)
        return float(getattr(sec, 'price', 0.0) or 0.0) if sec else 0.0

    def multiplier(self, ticker: str) -> float:
        sec = self.algo.securities.get(ticker)
        props = getattr(sec, 'symbol_properties', None) if sec is not None else None
        try:
            return float(getattr(props, 'contract_multiplier', 1.0) or 1.0)
        except (TypeError, ValueError):
            return 1.0

    def _bp_model(self, ticker):
        sec = self.algo.securities.get(ticker)
        if sec is None:
            return None
        return getattr(sec, 'buying_power_model', None) or getattr(sec, 'margin_model', None)

    def margin_fraction(self, ticker: str) -> float:
        """Initial margin per unit of position value."""
        model = self._bp_model(ticker)
        if model is not None and hasattr(model, 'margin_fraction'):
            from .models import NullBuyingPowerModel, ConstantBuyingPowerModel
            from .futures import FutureMarginModel
            if isinstance(model, (NullBuyingPowerModel, ConstantBuyingPowerModel, FutureMarginModel)):
                return model.margin_fraction(self.algo.securities[ticker])
        return 1.0 / self.leverage_of(ticker)

    def requirement(self, positions: Dict[str, float], price=None) -> float:
        """Initial margin of a set of positions ({ticker: qty}); `price(ticker)` overrides prices."""
        price = price or self._price
        used = 0.0
        covered = set()
        if self.group_margin is not None:
            req, covered = self.group_margin(positions, price)
            used += req
        for t, q in positions.items():
            if q and t not in covered:
                used += abs(q * price(t) * self.multiplier(t)) * self.margin_fraction(t)
        return used

    def margin_used(self, positions: Optional[Dict[str, float]] = None) -> float:
        if positions is None:
            positions = {t: h.quantity for t, h in self.algo.portfolio._held() if h.quantity}
        return self.requirement(positions)

    def pending_quantity(self, ticker: str, immediate_only: bool = True) -> float:
        q = 0.0
        for tk in self.book:
            if str(tk.symbol).upper() == ticker and tk.status in _OPEN:
                if not immediate_only or tk.order_type in _IMMEDIATE:
                    q += tk.quantity - tk.quantity_filled
        return q

    def pending_quantities(self, immediate_only: bool = True) -> Dict[str, float]:
        """pending_quantity() of every ticker with open orders, in one pass over the book."""
        out = {}
        for tk in self.book:
            if tk.status in _OPEN and (not immediate_only or tk.order_type in _IMMEDIATE):
                k = str(tk.symbol).upper()
                out[k] = out.get(k, 0.0) + (tk.quantity - tk.quantity_filled)
        return out

    def _estimated_cost(self, quantity: float, price: float) -> float:
        return abs(quantity * price) * (self.commission_rate + self.slippage_rate)

    # ---- submission --------------------------------------------------------

    def now(self):
        return self.algo.time

    def _reject(self, ticket, reason: str, code: int = _EC.PROCESSING_ERROR):
        ticket.status = OrderStatus.INVALID
        ticket.message = reason
        req = ticket.submit_request
        if req is not None:
            req.set_response(OrderResponse.error(req, code, reason))
        from .orders import PRE_ORDER_CHECK_CODES
        if code in PRE_ORDER_CHECK_CODES and ticket.status != OrderStatus.FILLED and not ticket.order_events:
            # LEAN pre-order check: the request never becomes an order (no id kept, no order event)
            self.prechecks.append((ticket, reason))
            self.algo._forget_order(ticket)
            return ticket
        self.rejected.append((ticket, reason))
        self._emit(ticket, OrderStatus.INVALID, 0.0, 0.0, 0.0, reason)
        return ticket

    def submit(self, ticket):
        ticker = str(ticket.symbol).upper()
        ticket.message = ''
        ticket._broker = self
        ticket.created_time = self.algo.utc_time
        req = ticket.submit_request
        if req is not None:
            req.order_id = ticket.order_id
        if getattr(self.algo, '_is_warming_up', False):
            self.ignored_warmup += 1
            ticket.status = OrderStatus.INVALID
            ticket.message = 'Orders are ignored during warm-up'
            if req is not None:
                req.set_response(OrderResponse.error(req, _EC.ALGORITHM_WARMING_UP,
                                                     'This operation is not allowed in Initialize or during warm up'))
            self.algo._forget_order(ticket)
            return ticket
        if not ticket.quantity:
            return self._reject(ticket, 'Order quantity is zero', _EC.ORDER_QUANTITY_ZERO)
        if ticker not in self.algo.securities or self._price(ticker) <= 0:
            return self._reject(ticket, f'No price for {ticker}: it is not subscribed or has no data yet',
                                _EC.SECURITY_PRICE_ZERO)
        if ticket.order_type not in _SIMULATED:
            self.unsimulated_types.add(ticket.order_type.name)
        hook = getattr(self.algo, '_pre_submit_check', None)
        if hook is not None:
            refused = hook(ticket)
            if refused:
                code, msg = refused
                return self._reject(ticket, msg, code)
        reason = None if getattr(ticket, '_skip_bp', False) else self._buying_power_shortfall(ticker, ticket)
        if reason:
            return self._reject(ticket, reason, _EC.INSUFFICIENT_BUYING_POWER)
        ticket.status = OrderStatus.SUBMITTED
        if req is not None:
            req.set_response(OrderResponse.success(req))
        # First session date the order may fill on. The runner sets it per phase: orders
        # placed after seeing bar t (on_data, end-of-day events) fill from the next bar;
        # orders placed before bar t is known (morning scheduled events) fill from bar t.
        ticket._fill_from = getattr(self.algo, '_next_fill_date', None)
        ticket.submit_time = self.algo.time
        if ticket.order_type == OrderType.TRAILING_STOP:
            self._init_trailing_stop(ticket, self._price(ticker))
        self.book.append(ticket)
        self._emit(ticket, OrderStatus.SUBMITTED, 0.0, 0.0, 0.0)
        return ticket

    # ---- updates and cancels (LEAN transaction handler rules) --------------

    def update(self, ticket, req):
        if ticket.status == OrderStatus.NEW:
            req.set_response(OrderResponse.error(req, _EC.INVALID_NEW_ORDER_STATUS,
                                                 f'Unable to update order with id {ticket.order_id} before it is submitted.'))
            return req.response
        closed = ticket.status not in _OPEN
        if closed and not req.is_allowed_for_closed_order():
            req.set_response(OrderResponse.error(req, _EC.INVALID_ORDER_STATUS,
                                                 f'Unable to update order with id {ticket.order_id} and status '
                                                 f'{ticket.status.name}'))
            return req.response
        if req.quantity is not None and float(req.quantity) == 0:
            req.set_response(OrderResponse.error(req, _EC.ORDER_QUANTITY_ZERO,
                                                 f'Unable to update order with id {ticket.order_id} to quantity zero'))
            return req.response
        if getattr(self.algo, '_is_warming_up', False):
            req.set_response(OrderResponse.error(req, _EC.ALGORITHM_WARMING_UP,
                                                 'This operation is not allowed in Initialize or during warm up'))
            return req.response
        check = getattr(self.algo, '_pre_update_check', None)
        if check is not None and not closed:
            refused = check(ticket, req)
            if refused:
                code, msg = refused
                req.set_response(OrderResponse.error(req, code, msg))
                return req.response
        if req.tag is not None:
            ticket.tag = req.tag
        if closed:  # tag-only update of a closed order: no brokerage round trip, no event
            req.set_response(OrderResponse.success(req))
            ticket.last_update_time = self.algo.utc_time
            return req.response
        if req.quantity is not None:
            ticket.quantity = float(req.quantity)
        if req.limit_price is not None:
            ticket.limit_price = float(req.limit_price)
        if req.stop_price is not None:
            ticket.stop_price = float(req.stop_price)
        if req.trigger_price is not None:
            ticket.trigger_price = float(req.trigger_price)
        if req.trailing_amount is not None:
            ticket.trailing_amount = float(req.trailing_amount)
            if ticket.order_type == OrderType.TRAILING_STOP:
                ticket.stop_price = 0
                self._init_trailing_stop(ticket, self._price(str(ticket.symbol).upper()))
        req.set_response(OrderResponse.success(req))
        ticket.last_update_time = self.algo.utc_time
        ticket.status = OrderStatus.UPDATE_SUBMITTED
        self._emit(ticket, OrderStatus.UPDATE_SUBMITTED, 0.0, 0.0, 0.0)
        return req.response

    def cancel(self, ticket, req):
        if ticket.status not in _OPEN:
            req.set_response(OrderResponse.error(req, _EC.INVALID_ORDER_STATUS,
                                                 f'Unable to cancel order with id {ticket.order_id} that is already '
                                                 f'{ticket.status.name}.'))
            return req.response
        if req.tag:
            ticket.tag = req.tag
        req.set_response(OrderResponse.success(req))
        self._cancel(ticket, 'Canceled')
        return req.response

    def _cancel(self, ticket, message):
        ticket.status = OrderStatus.CANCELED
        ticket.canceled_time = self.algo.utc_time
        self.book = [tk for tk in self.book if tk is not ticket]
        self._emit(ticket, OrderStatus.CANCELED, 0.0, 0.0, 0.0, message)

    def _buying_power_shortfall(self, ticker: str, ticket) -> Optional[str]:
        """Buying-power check of a new order: the security's own buying-power model when the
        strategy set one that decides it (null model, or a has_sufficient_buying_power_for_order
        override), else the engine rule below."""
        from .models import (NullBuyingPowerModel, BuyingPowerModel, is_overridden,
                             HasSufficientBuyingPowerForOrderParameters)
        model = self._bp_model(ticker)
        if isinstance(model, NullBuyingPowerModel):
            return None
        if model is not None and is_overridden(model, BuyingPowerModel, 'has_sufficient_buying_power_for_order'):
            self.model_notes.add((ticker, 'buying power'))
            res = model.has_sufficient_buying_power_for_order(
                HasSufficientBuyingPowerForOrderParameters(self.algo.portfolio, self.algo.securities[ticker], ticket))
            if getattr(res, 'is_sufficient', bool(res)):
                return None
            return getattr(res, 'reason', '') or 'Insufficient buying power (the strategy\'s buying power model)'
        return self._engine_buying_power_shortfall(ticker, ticket)

    def _engine_buying_power_shortfall(self, ticker: str, ticket) -> Optional[str]:
        """Reject an order that would push initial margin above equity. Orders that only
        reduce an existing position are always accepted."""
        held = self.algo.portfolio[ticker].quantity if ticker in self.algo.portfolio else 0.0
        before = held + self.pending_quantity(ticker)
        after = before + ticket.quantity
        if abs(after) <= abs(before) and (after == 0 or (after > 0) == (before > 0)):
            return None
        px = self._price(ticker)
        if ticket.order_type == OrderType.LIMIT and ticket.limit_price:
            px = float(ticket.limit_price)
        elif ticket.order_type in (OrderType.STOP_MARKET, OrderType.STOP_LIMIT) and ticket.stop_price:
            px = float(ticket.stop_price)
        positions = {t: h.quantity for t, h in self.algo.portfolio._held() if h.quantity}
        for t, pq in self.pending_quantities().items():
            if pq:
                positions[t] = positions.get(t, 0.0) + pq
        positions[ticker] = after
        used = self.requirement(positions, lambda t: px if t == ticker else self._price(t))
        equity = self.algo.portfolio.total_portfolio_value - self._estimated_cost(ticket.quantity * self.multiplier(ticker), px)
        if used > equity * (1 + 1e-9):
            return (f'Insufficient buying power: the position would need {used:,.2f} of margin '
                    f'(leverage {self.leverage_of(ticker):g}x) but equity is {equity:,.2f}')
        return None

    def _fill_time_shortfall(self, tk, ticker: str, px: float) -> Optional[str]:
        """Buying power is checked again when an order that adds exposure fills (LEAN does this
        for resting orders): resting limit/stop orders accepted earlier must not fill past the
        margin limit. Market-type orders get 5% of slack for the overnight gap between the close
        they were sized on and the open they fill at (LEAN fills them at the sizing price)."""
        from .models import NullBuyingPowerModel, BuyingPowerModel, is_overridden
        model = self._bp_model(ticker)
        if isinstance(model, NullBuyingPowerModel) or (
                model is not None and is_overridden(model, BuyingPowerModel, 'has_sufficient_buying_power_for_order')):
            return None
        held = self.algo.portfolio[ticker].quantity if ticker in self.algo.portfolio else 0.0
        fill_q = tk.quantity - tk.quantity_filled
        after = held + fill_q
        if abs(after) <= abs(held) and (after == 0 or (after > 0) == (held > 0)):
            return None
        positions = {t: h.quantity for t, h in self.algo.portfolio._held() if h.quantity}
        positions[ticker] = after
        used = self.requirement(positions, lambda t: px if t == ticker else self._price(t))
        pf = self.algo.portfolio
        m = self.multiplier(ticker)
        equity = (pf.total_portfolio_value + held * (px - self._price(ticker)) * m
                  - abs(fill_q * px * m) * self.commission_rate)
        slack = 1.05 if tk.order_type in _IMMEDIATE else 1.0
        if used > equity * slack + 1e-9:
            return (f'Insufficient buying power to complete the order at fill: it would need {used:,.2f} '
                    f'of margin but equity is {equity:,.2f}')
        return None

    def cancel_open(self, ticker: Optional[str] = None, tag: str = ''):
        for tk in list(self.book):
            if tk.status in _OPEN and (ticker is None or str(tk.symbol).upper() == ticker):
                tk.tag = tag or tk.tag
                self._cancel(tk, 'Canceled')

    # ---- fills -------------------------------------------------------------

    def _emit(self, ticket, status, price, qty, fee, message='', when=None, is_assignment=False):
        ev = OrderEvent(ticket.order_id, ticket.symbol, status, price, qty, message=message)
        ev.order_fee = _OrderFee(fee)
        ev.utc_time = when if when is not None else self.algo.time
        ev.quantity = ticket.quantity
        ev.direction = 0 if ticket.quantity > 0 else 1
        ev.limit_price = ticket.limit_price or None
        ev.stop_price = ticket.stop_price or None
        ev.trigger_price = getattr(ticket, 'trigger_price', 0) or None
        ev.trailing_amount = getattr(ticket, 'trailing_amount', 0) or None
        ev.is_assignment = is_assignment
        ev.ticket = ticket
        ev.id = len(ticket.order_events) + 1
        ticket.order_events.append(ev)
        handler = getattr(self.algo, 'on_order_event', None)
        if handler is not None:
            handler(ev)
        return ev

    def _slipped(self, raw: float, qty: float, tk=None) -> float:
        sec = self.algo.securities.get(str(tk.symbol).upper()) if tk is not None else None
        model = getattr(sec, 'slippage_model', None) if sec is not None else None
        if model is not None:
            self.model_notes.add((str(tk.symbol).upper(), 'slippage'))
            amount = abs(float(model.get_slippage_approximation(
                sec, self.algo.transactions.get_order_by_id(tk.order_id) or tk) or 0.0))
            return raw + amount if qty > 0 else max(0.0, raw - amount)
        f = self._slip_fraction(str(tk.symbol).upper()) if tk is not None else self.slippage_rate
        return raw * (1.0 + f) if qty > 0 else raw * (1.0 - f)

    def _slip_fraction(self, ticker: str) -> float:
        """Adverse fraction of the run's cost model for a fill of `ticker` on the current bar."""
        cm = self.cost_model
        if cm is None or cm.slippage_type == 'pct_price':
            return self.slippage_rate
        half = float(cm.spread_bps or 0.0) / 2e4
        if cm.slippage_type != 'vol_scaled':
            return half
        return float(cm.slippage_value) * self._daily_vol(ticker) + half

    def _daily_vol(self, ticker: str, window: int = 20) -> float:
        """Std of the close-to-close returns of the `window` bars BEFORE the fill bar (known before it
        trades; base.costs.daily_vol's definition)."""
        day = self._day
        key = (ticker, day)
        if key in self._vol_cache:
            return self._vol_cache[key]
        v = 0.0
        session = getattr(self.algo, '_session', None)
        s = session.series.get(ticker) if session is not None else None
        if s is not None and day is not None:
            i = s.index_of_date(day)
            if i is None:
                i = s.count_known_at(day)
            c = s.close[max(0, i - window - 1):i]
            r = [c[j] / c[j - 1] - 1.0 for j in range(1, len(c)) if c[j - 1]]
            if len(r) >= 2:
                m = sum(r) / len(r)
                v = math.sqrt(sum((x - m) ** 2 for x in r) / (len(r) - 1))
        self._vol_cache[key] = v
        return v

    def _model_commission(self, q: float, px: float, m: float) -> float:
        cm = self.cost_model
        units = abs(q) * m
        notional = units * px
        if cm.commission_type == 'pct_notional':
            fee = notional * cm.commission_value
        elif cm.commission_type == 'per_share':
            fee = units * cm.commission_value
        else:
            fee = cm.commission_value if units > 0 else 0.0
        if cm.min_commission and units > 0:
            fee = max(fee, cm.min_commission)
        if cm.max_commission_pct is not None:
            fee = min(fee, notional * float(cm.max_commission_pct))
        return abs(float(fee))

    def _fee(self, tk, ticker, px, q) -> float:
        sec = self.algo.securities.get(ticker)
        model = getattr(sec, 'fee_model', None) if sec is not None else None
        if model is not None:
            from .models import OrderFeeParameters
            self.model_notes.add((ticker, 'fees'))
            fee = model.get_order_fee(OrderFeeParameters(sec, self.algo.transactions.get_order_by_id(tk.order_id) or tk))
            amount = getattr(getattr(fee, 'value', fee), 'amount', fee)
            try:
                return abs(float(amount))
            except (TypeError, ValueError):
                return 0.0
        if self.cost_model is not None:
            return self._model_commission(q, px, self.multiplier(ticker))
        return abs(q * px * self.multiplier(ticker)) * self.commission_rate

    def _fill_price(self, tk, o, h, l, c) -> Optional[tuple]:
        """(price, slippage_applies) or None when the order does not fill on this bar."""
        q = tk.quantity
        ot = tk.order_type
        if ot == OrderType.MARKET:
            return (c if self.fill_timing == 'next_close' else o), True
        if ot == OrderType.MARKET_ON_OPEN:
            return o, True
        if ot == OrderType.MARKET_ON_CLOSE:
            return c, True
        if ot == OrderType.LIMIT or (ot == OrderType.STOP_LIMIT and getattr(tk, '_stop_triggered', False)):
            lim = float(tk.limit_price or 0)
            if lim <= 0:
                return None
            if q > 0:
                if o <= lim:
                    return o, False
                if l <= lim:
                    return lim, False
            else:
                if o >= lim:
                    return o, False
                if h >= lim:
                    return lim, False
            return None
        if ot == OrderType.STOP_MARKET:
            stop = float(tk.stop_price or 0)
            if stop <= 0:
                return None
            if q > 0:
                if o >= stop:
                    return o, True
                if h >= stop:
                    return stop, True
            else:
                if o <= stop:
                    return o, True
                if l <= stop:
                    return stop, True
            return None
        if ot == OrderType.STOP_LIMIT:
            stop, lim = float(tk.stop_price or 0), float(tk.limit_price or 0)
            if stop <= 0 or lim <= 0:
                return None
            gap = (o >= stop) if q > 0 else (o <= stop)
            touched = (h >= stop) if q > 0 else (l <= stop)
            if gap and ((q > 0 and o <= lim) or (q < 0 and o >= lim)):
                return o, False
            if touched:
                tk._stop_triggered = True  # rests as a limit order from the next bar
            return None
        if ot == OrderType.TRAILING_STOP:
            stop = float(tk.stop_price or 0)
            if stop > 0:
                if q > 0:
                    if o >= stop:
                        return o, True
                    if h >= stop:
                        return stop, True
                else:
                    if o <= stop:
                        return o, True
                    if l <= stop:
                        return stop, True
            # not triggered: the stop follows the bar's extreme in the favourable direction
            # (a sell stop follows highs, a buy stop follows lows)
            self._trail(tk, l if q > 0 else h)
            return None
        if ot == OrderType.LIMIT_IF_TOUCHED:
            trig, lim = float(getattr(tk, 'trigger_price', 0) or 0), float(tk.limit_price or 0)
            if trig <= 0 or lim <= 0:
                return None
            if getattr(tk, 'trigger_touched', False):
                if q > 0:
                    if o <= lim:
                        return o, False
                    if l <= lim:
                        return lim, False
                else:
                    if o >= lim:
                        return o, False
                    if h >= lim:
                        return lim, False
                return None
            # buy: triggered when the price falls to the trigger; sell: when it rises to it
            gap = (o <= trig) if q > 0 else (o >= trig)
            touched = (l <= trig) if q > 0 else (h >= trig)
            if gap and ((q > 0 and o <= lim) or (q < 0 and o >= lim)):
                tk.trigger_touched = True
                return o, False
            if touched:
                tk.trigger_touched = True  # rests as a limit order from the next bar
            return None
        return None

    # ---- trailing stops ----------------------------------------------------

    @staticmethod
    def _stop_from(price, amount, as_pct, quantity):
        if quantity < 0:   # sell stop below the market
            return price * (1 - amount) if as_pct else price - amount
        return price * (1 + amount) if as_pct else price + amount

    def _init_trailing_stop(self, tk, price):
        amount = float(getattr(tk, 'trailing_amount', 0) or 0)
        if amount <= 0 or price <= 0:
            return
        if not tk.stop_price:
            tk.stop_price = self._stop_from(price, amount, tk.trailing_as_percentage, tk.quantity)

    def _trail(self, tk, extreme):
        amount = float(getattr(tk, 'trailing_amount', 0) or 0)
        if amount <= 0 or extreme <= 0:
            return
        cand = self._stop_from(extreme, amount, tk.trailing_as_percentage, tk.quantity)
        if tk.quantity < 0 and cand > (tk.stop_price or 0):
            tk.stop_price = cand
        elif tk.quantity > 0 and (not tk.stop_price or cand < tk.stop_price):
            tk.stop_price = cand

    def submit_combo(self, tickets):
        """A combo order: every leg passes the per-leg checks, then the buying-power check is made
        once for all legs together (a spread is margined as a spread); all accepted or all rejected."""
        group = getattr(tickets[0], '_combo', None)
        held = {}
        for tk in tickets:
            tk._skip_bp = True
            self.submit(tk)
        ok = [tk for tk in tickets if tk.status == OrderStatus.SUBMITTED]
        if len(ok) != len(tickets):
            bad = next(tk for tk in tickets if tk.status != OrderStatus.SUBMITTED)
            why = (getattr(bad, 'message', '') or 'refused').rstrip('.')
            for tk in ok:
                self.book = [b for b in self.book if b is not tk]
                self._reject(tk, f'Combo order rejected: the {bad.symbol} leg was refused ({why})',
                             _EC.PROCESSING_ERROR)
            return tickets
        positions = {t: h.quantity for t, h in self.algo.portfolio._held() if h.quantity}
        for t, pq in self.pending_quantities().items():
            if pq:
                positions[t] = positions.get(t, 0.0) + pq
        used = self.requirement(positions)
        equity = self.algo.portfolio.total_portfolio_value
        if used > equity * (1 + 1e-9):
            for tk in tickets:
                self.book = [b for b in self.book if b is not tk]
                self._reject(tk, f'Insufficient buying power: the combo would need {used:,.2f} of margin but '
                                 f'equity is {equity:,.2f}', _EC.INSUFFICIENT_BUYING_POWER)
        return tickets

    def _combo_fill(self, group, bars, day):
        """Fill every leg of a combo on this bar, or none of them."""
        legs = [tk for tk in group['legs'] if tk.status in _OPEN]
        if not legs:
            return
        prices = {}
        for tk in legs:
            b = bars.get(str(tk.symbol).upper())
            if b is None:
                return
            if tk.order_type == OrderType.COMBO_LEG_LIMIT:
                saved = tk.order_type
                tk.order_type = OrderType.LIMIT
                res = self._quote_aware_fill_price(tk, b)
                tk.order_type = saved
                if res is None:
                    return
            else:
                saved = tk.order_type
                tk.order_type = OrderType.MARKET
                res = self._quote_aware_fill_price(tk, b)
                tk.order_type = saved
            prices[id(tk)] = res
        if group['type'] == OrderType.COMBO_LIMIT:
            qty = group['quantity'] or 1.0
            net = sum((tk.quantity / qty) * prices[id(tk)][0] for tk in legs)
            lim = float(group.get('limit_price') or 0.0)
            if (qty > 0 and net > lim) or (qty < 0 and net < lim):
                return
        when = day + (timedelta(hours=16) if self.fill_timing == 'next_close' else timedelta(hours=9, minutes=30))
        for tk in legs:
            raw, slip = prices[id(tk)]
            px = self._slipped(raw, tk.quantity, tk) if slip else raw
            self._fill(tk, str(tk.symbol).upper(), px, raw, when)

    def fill_same_close(self, day: datetime, bars: Dict[str, Dict]):
        """fill_timing 'same_close': MARKET orders placed once `day`'s bar was known (on_data, the
        framework, events at / after the close) fill at that bar's close. Others wait as usual."""
        if self.fill_timing != 'same_close' or not self.book:
            return
        self._day = day
        nxt = day + timedelta(days=1)
        remaining = []
        marks = {}
        for tk in self.book:
            if tk.status not in _OPEN:
                continue
            if (tk.order_type != OrderType.MARKET or getattr(tk, '_combo', None) is not None
                    or getattr(tk, '_fill_from', None) != nxt):
                remaining.append(tk)
                continue
            ticker = str(tk.symbol).upper()
            b = bars.get(ticker)
            if b is None or self._custom_fill(tk, ticker, b, day):
                if tk.status in _OPEN:
                    remaining.append(tk)
                continue
            raw = b['close']
            if 'half_spread' in b:
                hs = b['half_spread'](raw) if callable(b['half_spread']) else float(b['half_spread'])
                raw = raw + hs if tk.quantity > 0 else max(0.0, raw - hs)
            px = self._slipped(raw, tk.quantity, tk)
            reason = self._fill_time_shortfall(tk, ticker, px)
            if reason:
                self._reject(tk, reason)
                continue
            self._fill(tk, ticker, px, raw, day + timedelta(hours=16))
            marks[ticker] = b['close']
        self.book = [tk for tk in remaining if tk.status in _OPEN]
        if marks:   # a fill marks the holding at its (slipped) price: back to the close for the equity point
            self.algo.portfolio.update_market_prices(marks)

    # ---- carry: money that changes a position's P&L between fills ------------------

    def record_carry(self, ticker: str, kind: str, amount: float):
        """Borrow fees (kind 'borrow', a cost) or cash dividends ('dividends', income) of a held
        position; attached to the symbol's next fill (or its open position at the end) so the trade
        list adds up to the equity change."""
        d = self.carry.setdefault(str(ticker).upper(), {})
        d[kind] = d.get(kind, 0.0) + float(amount)

    def append_fill(self, row: Dict):
        """A fill booked outside the order book (option exercise / assignment / expiry): the contract multiplier
        and the position's carry are attached as on _fill's rows, so trades are priced in money like the equity."""
        ticker = str(row.get('symbol', '')).upper()
        m = self.multiplier(ticker)
        if m != 1.0:
            row.setdefault('multiplier', m)
        carry = self.carry.pop(ticker, None)
        if carry:
            row.update({k: v for k, v in carry.items() if v})
        self.fills.append(row)

    def record_split(self, ticker: str, ratio: float, when: datetime):
        """A split applied to a RAW-mode holding (quantity x ratio, average price / ratio)."""
        self.corporate_events.append({'time': when.strftime('%Y-%m-%d 00:00:00'), 'symbol': str(ticker).upper(),
                                      'split': float(ratio)})

    def accrue_borrow(self, now: datetime):
        """After the close: the cost model's annual borrow fee on the market value of every short
        position, for the calendar days since the previous accrual (ACT/day_count), paid from cash."""
        last, self._last_accrual = self._last_accrual, now
        if self.borrow_rate <= 0 or last is None:
            return
        days = (now - last).total_seconds() / 86400.0
        if days <= 0:
            return
        pf = self.algo.portfolio
        for t, h in pf._held():
            if h.quantity >= 0:
                continue
            sec = self.algo.securities.get(t)
            st = getattr(getattr(sec, 'symbol', None), 'security_type', SecurityType.EQUITY)
            if st not in (SecurityType.EQUITY, SecurityType.CFD):
                continue    # futures / options / forex shorts are not borrowed securities
            value = abs(h.quantity * float(getattr(h, 'market_price', 0.0) or self._price(t)) * self.multiplier(t))
            value *= pf._rate(pf.quote_currency(t)) or 1.0
            fee = value * self.borrow_rate * days / self.day_count
            if fee > 0:
                pf._cash -= fee
                self.total_borrow += fee
                self.record_carry(t, 'borrow', fee)

    def process_bar(self, day: datetime, bars: Dict[str, Dict]):
        """Fill open orders on `day`'s bars (each order was placed before this bar)."""
        self._day = day
        if not self.book:
            return
        remaining = []
        done_groups = set()
        for tk in self.book:
            group = getattr(tk, '_combo', None)
            if group is not None and tk.status in _OPEN:
                fill_from = getattr(tk, '_fill_from', None)
                if (fill_from is None or day >= fill_from) and id(group) not in done_groups:
                    done_groups.add(id(group))
                    self._combo_fill(group, bars, day)
                if tk.status in _OPEN:
                    remaining.append(tk)
                continue
            if tk.status not in _OPEN:
                continue
            fill_from = getattr(tk, '_fill_from', None)
            if fill_from is not None and day < fill_from:
                remaining.append(tk)  # placed after this bar was known: a later bar fills it
                continue
            ticker = str(tk.symbol).upper()
            b = bars.get(ticker)
            if b is None or tk.order_type not in _SIMULATED:
                remaining.append(tk)
                continue
            if self._custom_fill(tk, ticker, b, day):
                if tk.status in _OPEN:
                    remaining.append(tk)
                continue
            res = self._quote_aware_fill_price(tk, b)
            if res is None:
                tif = getattr(tk, 'time_in_force', None)
                exp = getattr(tif, 'expiry', None)
                if exp is not None and day.date() >= (exp.date() if hasattr(exp, 'date') else exp):
                    tk.status = OrderStatus.CANCELED
                    tk.canceled_time = self.algo.utc_time
                    self._emit(tk, OrderStatus.CANCELED, 0.0, 0.0, 0.0, 'Expired (time in force: good til date)')
                    continue
                if str(tif).lower() == 'day':
                    tk.status = OrderStatus.CANCELED
                    tk.canceled_time = self.algo.utc_time
                    self._emit(tk, OrderStatus.CANCELED, 0.0, 0.0, 0.0, 'Expired (time in force: day)')
                    continue
                remaining.append(tk)
                continue
            raw, slip = res
            px = self._slipped(raw, tk.quantity, tk) if slip else raw
            reason = self._fill_time_shortfall(tk, ticker, px)
            if reason:
                self._reject(tk, reason)
                continue
            on_close = tk.order_type == OrderType.MARKET_ON_CLOSE or (
                tk.order_type == OrderType.MARKET and self.fill_timing == 'next_close')
            self._fill(tk, ticker, px, raw, day + (timedelta(hours=16) if on_close else timedelta(hours=9, minutes=30)))
        self.book = [tk for tk in remaining if tk.status in _OPEN]

    def _quote_aware_fill_price(self, tk, b):
        """Fill price from the bar; instruments with modelled quotes (options) buy at the ask and
        sell at the bid of the reference price."""
        res = self._fill_price(tk, b['open'], b['high'], b['low'], b['close'])
        if res is None or 'half_spread' not in b:
            return res
        raw, slip = res
        hs = b['half_spread'](raw) if callable(b['half_spread']) else float(b['half_spread'])
        return (raw + hs if tk.quantity > 0 else max(0.0, raw - hs)), slip

    def _custom_fill(self, tk, ticker, b, day) -> bool:
        """A strategy fill model (security.set_fill_model) decides the fill. Returns True when it
        handled the order on this bar."""
        sec = self.algo.securities.get(ticker)
        model = getattr(sec, 'fill_model', None) if sec is not None else None
        if model is None:
            return False
        from .models import FillModel, FillModelParameters, is_overridden
        names = ('fill', 'market_fill', 'limit_fill', 'stop_market_fill', 'stop_limit_fill', 'market_on_open_fill',
                 'market_on_close_fill', 'limit_if_touched_fill', 'trailing_stop_fill')
        if not any(is_overridden(model, FillModel, n) for n in names):
            return False
        self.model_notes.add((ticker, 'fills'))
        from .orders import Order
        tkt = lambda order: order.__dict__.get('_ticket', order) if isinstance(order, Order) else order
        FillModel._context = {'engine': lambda order: self._quote_aware_fill_price(tkt(order), b),
                              'slip': lambda order, px: self._slipped(px, order.quantity, tkt(order))}
        try:
            fill = model.fill(FillModelParameters(sec, self.algo.transactions.get_order_by_id(tk.order_id) or tk))
        finally:
            FillModel._context = None
        events = list(fill) if isinstance(fill, (list, tuple)) else [fill]
        when = day + (timedelta(hours=16) if tk.order_type == OrderType.MARKET_ON_CLOSE else timedelta(hours=9, minutes=30))
        done = False
        for ev in events:
            if ev is None:
                continue
            q = float(getattr(ev, 'fill_quantity', 0.0) or 0.0)
            st = getattr(ev, 'status', None)
            if q and st in (OrderStatus.FILLED, OrderStatus.PARTIALLY_FILLED):
                remaining = tk.quantity - tk.quantity_filled
                if abs(q) > abs(remaining):
                    q = remaining
                px = float(ev.fill_price)
                fee = getattr(getattr(getattr(ev, 'order_fee', None), 'value', None), 'amount', None)
                self._fill(tk, ticker, px, px, when, quantity=q, fee=fee)
                done = True
            elif st == OrderStatus.CANCELED:
                self._cancel(tk, getattr(ev, 'message', '') or 'Canceled by the fill model')
                done = True
        return done or True

    def _fill(self, tk, ticker, px, raw, when, quantity=None, fee=None):
        q = (tk.quantity - tk.quantity_filled) if quantity is None else quantity
        if fee is None:
            fee = self._fee(tk, ticker, px, q)
        m = self.multiplier(ticker)
        self.total_fees += fee
        self.total_slippage += abs(q) * abs(px - raw) * m
        self.algo.portfolio.process_fill(ticker, q, px, fee)
        filled_before = tk.quantity_filled
        tk.average_fill_price = ((tk.average_fill_price * filled_before + px * q) / (filled_before + q)
                                 if (filled_before + q) else px)
        tk.quantity_filled = filled_before + q
        tk.status = OrderStatus.FILLED if abs(tk.quantity_filled - tk.quantity) < 1e-12 else OrderStatus.PARTIALLY_FILLED
        tk.fill_time = when
        tk.last_fill_time = when
        tk.order_fee = getattr(tk, 'order_fee', 0.0) + fee if filled_before else fee
        row = {
            'time': when.strftime('%Y-%m-%d 00:00:00'),
            'symbol': ticker,
            'quantity': q,
            'price': px,
            'raw_price': raw,
            'commission': fee,
            'slippage': abs(q) * abs(px - raw) * m,
            'type': tk.order_type.name,
            'order_id': tk.order_id,
            'tag': tk.tag or '',
        }
        if m != 1.0:
            row['multiplier'] = m
        carry = self.carry.pop(ticker, None)
        if carry:
            row.update({k: v for k, v in carry.items() if v})   # accrued on the position BEFORE this fill
        self.fills.append(row)
        self._emit(tk, tk.status, px, q, fee, when=when)

    # ---- margin call -------------------------------------------------------

    def check_margin(self, now: datetime):
        """After marking to the close: if margin used exceeds equity, queue market orders
        that cut every position proportionally back to 95% of equity (filled next open)."""
        pf = self.algo.portfolio
        positions = {t: h.quantity for t, h in pf.items() if h.quantity}
        if not positions:
            return
        if any(tk.status in _OPEN and str(tk.tag).startswith('Margin call') for tk in self.book):
            return
        equity = pf.total_portfolio_value
        used = self.margin_used(positions)
        if used <= equity:
            return
        frac = 1.0 if equity <= 0 else min(1.0, 1.0 - 0.95 * equity / used)
        self.margin_calls += 1
        for t, q in positions.items():
            lot = lot_size(getattr(getattr(self.algo.securities.get(t), 'symbol', None), 'security_type', None))
            cut = -q if frac >= 1.0 else -round_to_lot(q * frac, lot)
            if cut == 0:
                cut = -math.copysign(lot, q)
            tk = self.algo._make_ticket(t, cut, OrderType.MARKET, tag='Margin call')
            tk.status = OrderStatus.SUBMITTED
            tk._fill_from = datetime(now.year, now.month, now.day) + timedelta(days=1)
            self.book.append(tk)
