# ============================================================================
# Fincept Terminal - Strategy Engine: pluggable reality models (LEAN API)
#
# Fee, fill, slippage, buying-power, margin-call, margin-interest, settlement and
# brokerage models as real objects. The broker consults the models a strategy sets on a
# security (security.set_fee_model(...), set_fill_model, set_slippage_model,
# set_buying_power_model / set_margin_model, set_margin_interest_rate_model) or on the
# algorithm (set_brokerage_model with can_submit_order / can_update_order / get_benchmark).
# The base classes implement the engine's own behaviour, so a strategy subclass that calls
# super() gets the engine's numbers and can adjust them, as in LEAN.
#
# Without a model from the strategy, the run's commission and slippage rates apply.
# ============================================================================

import math
from datetime import datetime, timedelta
from typing import Optional

from .enums import OrderType, OrderStatus, SecurityType


# ---------------------------------------------------------------- money

class CashAmount:
    def __init__(self, amount=0.0, currency="USD"):
        self.amount = float(amount)
        self.currency = currency

    def __repr__(self):
        return f"{self.amount:g} {self.currency}"

    Amount = property(lambda self: self.amount)
    Currency = property(lambda self: self.currency)


CashAmount.ZERO = CashAmount(0.0, "USD")
CashAmount.Zero = CashAmount.ZERO


class OrderFee:
    def __init__(self, value=0.0, currency="USD"):
        self.value = value if isinstance(value, CashAmount) else CashAmount(value, currency)

    @staticmethod
    def zero(currency="USD"):
        return OrderFee(CashAmount(0.0, currency))

    def __repr__(self):
        return f"OrderFee({self.value})"

    Value = property(lambda self: self.value)


OrderFee.ZERO = OrderFee(0.0)
OrderFee.Zero = OrderFee.ZERO


# ---------------------------------------------------------------- parameters / results

class OrderFeeParameters:
    def __init__(self, security, order):
        self.security = security
        self.order = order

    Security = property(lambda self: self.security)
    Order = property(lambda self: self.order)


class FillModelParameters:
    def __init__(self, security, order, config_provider=None, stale_price_time_span=None,
                 securities_for_orders=None, on_order_updated=None):
        self.security = security
        self.order = order
        self.config_provider = config_provider
        self.stale_price_time_span = stale_price_time_span or timedelta(hours=1)
        self.securities_for_orders = securities_for_orders or {}


class Fill(list):
    """LEAN Fill: the order events produced by a fill model call."""
    def __init__(self, events=None):
        if events is None:
            events = []
        elif not isinstance(events, (list, tuple)):
            events = [events]
        super().__init__(events)

    @property
    def order_events(self):
        return list(self)


class GetMaximumOrderQuantityForTargetBuyingPowerParameters:
    def __init__(self, portfolio, security, target_buying_power, minimum_order_margin_portfolio_percentage=0.0,
                 silence_non_error_reasons=False):
        self.portfolio = portfolio
        self.security = security
        self.target_buying_power = float(target_buying_power)
        self.minimum_order_margin_portfolio_percentage = minimum_order_margin_portfolio_percentage
        self.silence_non_error_reasons = silence_non_error_reasons


class GetMaximumOrderQuantityResult:
    def __init__(self, quantity, reason=None, is_error=False):
        self.quantity = float(quantity)
        self.reason = reason or ''
        self.is_error = bool(is_error)

    Quantity = property(lambda self: self.quantity)


class HasSufficientBuyingPowerForOrderParameters:
    def __init__(self, portfolio, security, order):
        self.portfolio = portfolio
        self.security = security
        self.order = order


class HasSufficientBuyingPowerForOrderResult:
    def __init__(self, is_sufficient, reason=''):
        self.is_sufficient = bool(is_sufficient)
        self.reason = reason or ''

    IsSufficient = property(lambda self: self.is_sufficient)


class MaintenanceMargin:
    def __init__(self, value=0.0):
        self.value = float(value)


class InitialMargin:
    def __init__(self, value=0.0):
        self.value = float(value)

    @staticmethod
    def zero():
        return InitialMargin(0.0)


class BuyingPower:
    def __init__(self, value=0.0):
        self.value = float(value)


class ReservedBuyingPowerForPosition:
    def __init__(self, value=0.0):
        self.absolute_used_buying_power = float(value)


class ReservedBuyingPowerForPositionParameters:
    def __init__(self, security):
        self.security = security

    def result_in_account_currency(self, value):
        return ReservedBuyingPowerForPosition(value)


class MarginInterestRateParameters:
    def __init__(self, security, time):
        self.security = security
        self.time = time


# ---------------------------------------------------------------- fees

class FeeModel:
    """Base fee model: no fee (LEAN's FeeModel base)."""
    def get_order_fee(self, parameters):
        return OrderFee(CashAmount(0.0, "USD"))

    GetOrderFee = get_order_fee


class ConstantFeeModel(FeeModel):
    """A fixed fee per order."""
    def __init__(self, fee=0.0, currency="USD"):
        self._fee = float(fee)
        self._currency = currency

    def get_order_fee(self, parameters):
        return OrderFee(CashAmount(abs(self._fee), self._currency))


class InteractiveBrokersFeeModel(FeeModel):
    """IB fixed pricing for US equities: $0.005 per share, at least $1, at most 1% of the trade value."""
    def __init__(self, *args, **kwargs):
        pass

    def get_order_fee(self, parameters):
        order, sec = parameters.order, parameters.security
        q = abs(getattr(order, 'quantity', 0.0))
        price = float(getattr(sec, 'price', 0.0) or 0.0)
        fee = max(1.0, 0.005 * q)
        if price > 0:
            fee = min(fee, 0.01 * q * price)
        return OrderFee(CashAmount(fee, "USD"))


# ---------------------------------------------------------------- slippage

class SlippageModel:
    """Base slippage: none. get_slippage_approximation returns a price amount."""
    def get_slippage_approximation(self, asset, order):
        return 0.0

    GetSlippageApproximation = get_slippage_approximation


class NullSlippageModel(SlippageModel):
    pass


NullSlippageModel.INSTANCE = NullSlippageModel()


class ConstantSlippageModel(SlippageModel):
    """Slippage = a fixed fraction of the reference price (the open for market-on-open orders)."""
    def __init__(self, slippage_percent=0.0):
        self._pct = float(slippage_percent)

    def get_slippage_approximation(self, asset, order):
        last = asset.get_last_data() if hasattr(asset, 'get_last_data') else None
        if last is None:
            return 0.0
        ref = last.open if getattr(order, 'order_type', None) == OrderType.MARKET_ON_OPEN else last.close
        return float(ref) * self._pct


class VolumeShareSlippageModel(SlippageModel):
    """slippage = min(|qty| / bar volume, volume_limit)^2 * price_impact * price (LEAN's rule)."""
    def __init__(self, volume_limit=0.025, price_impact=0.1):
        self._limit = float(volume_limit)
        self._impact = float(price_impact)

    def get_slippage_approximation(self, asset, order):
        last = asset.get_last_data() if hasattr(asset, 'get_last_data') else None
        if last is None:
            return 0.0
        pct = self._limit * self._limit * self._impact
        vol = float(getattr(last, 'volume', 0.0) or 0.0)
        if vol <= 0:
            if asset.symbol.security_type in (SecurityType.FOREX, SecurityType.CFD, SecurityType.CRYPTO):
                return 0.0
        else:
            share = min(abs(float(getattr(order, 'quantity', 0.0))) / vol, self._limit)
            pct = share * share * self._impact
        ref = last.open if getattr(order, 'order_type', None) == OrderType.MARKET_ON_OPEN else last.close
        return pct * float(ref)


class MarketImpactSlippageModel(SlippageModel):
    """Almgren-style market impact (LEAN's parameters): permanent impact sigma*T*gamma*nu^alpha and
    temporary impact sigma*eta*nu^beta, nu = order size / execution time / average daily volume,
    sigma = std of daily returns over the last year, average volume over the last 10 days, from
    the algorithm's history. Deterministic: LEAN adds Gaussian noise and scales the impact by a
    uniform random fraction; this uses their expected values (no noise, fraction 0.5)."""
    def __init__(self, algorithm=None, non_negative=True, latency=0.075, impact_time=1800.0, alpha=0.891,
                 beta=0.600, gamma=0.314, eta=0.142, delta=0.267, random_seed=50):
        if latency <= 0 or impact_time <= 0:
            raise ValueError("Latency and impact time must be greater than 0.")
        self._algo = algorithm
        self._non_negative = non_negative
        day = 6.5 * 3600.0
        self._exec_t = latency / day
        self._impact_t = self._exec_t + impact_time / day
        self._alpha, self._beta, self._gamma, self._eta, self._delta = alpha, beta, gamma, eta, delta
        self._stats = {}

    def _symbol_stats(self, asset):
        key = (str(asset.symbol), getattr(self._algo, 'time', None) and self._algo.time.date())
        if key in self._stats:
            return self._stats[key]
        sigma = avg_vol = 0.0
        algo = self._algo
        if algo is not None:
            try:
                bars = algo.history[__import__('fincept_engine.types', fromlist=['TradeBar']).TradeBar](asset.symbol, 253)
                closes = [b.close for b in bars]
                vols = [b.volume for b in bars][-10:]
                rets = [(closes[i] - closes[i - 1]) / closes[i - 1] for i in range(1, len(closes)) if closes[i - 1]]
                if len(rets) > 1:
                    m = sum(rets) / len(rets)
                    sigma = math.sqrt(sum((r - m) ** 2 for r in rets) / (len(rets) - 1))
                avg_vol = sum(vols) / len(vols) if vols else 0.0
            except Exception:
                pass
        self._stats[key] = (sigma, avg_vol)
        return sigma, avg_vol

    def get_slippage_approximation(self, asset, order):
        if asset.symbol.security_type in (SecurityType.FOREX, SecurityType.CFD):
            raise ValueError(f"Asset of {asset.symbol.security_type.name} is not supported as "
                             f"MarketImpactSlippageModel requires volume data")
        sigma, avg_vol = self._symbol_stats(asset)
        if avg_vol <= 0:
            return 0.0
        nu = abs(float(order.quantity)) / self._exec_t / avg_vol
        permanent = sigma * self._exec_t * self._gamma * nu ** self._alpha
        temporary = sigma * self._eta * nu ** self._beta
        impact = (temporary + permanent * 0.5) * 0.5
        impact = min(impact, 1.0)
        if self._non_negative:
            impact = max(0.0, impact)
        last = asset.get_last_data()
        ref = last.open if (last is not None and order.order_type == OrderType.MARKET_ON_OPEN) else asset.price
        return impact * float(ref)


class AlphaStreamsSlippageModel(SlippageModel):
    """1 basis point of the price for equities (AlphaStreams' default)."""
    def get_slippage_approximation(self, asset, order):
        return 0.0001 * float(asset.price or 0.0)


# ---------------------------------------------------------------- fills

class FillModel:
    """Base fill model: the engine's daily-bar fill rules (see broker.py). A subclass may override
    market_fill / limit_fill / stop_market_fill / stop_limit_fill / market_on_open_fill /
    market_on_close_fill / limit_if_touched_fill / trailing_stop_fill, or fill(parameters), and
    return OrderEvents; calling super() gives the engine's fill for the current bar."""

    _context = None   # set by the broker while it fills one order on one bar

    def _engine_event(self, asset, order):
        ctx = FillModel._context
        from .types import OrderEvent
        ev = OrderEvent(order.order_id, order.symbol, OrderStatus.SUBMITTED, 0.0, 0.0)
        if ctx is None:
            return ev
        res = ctx['engine'](order)
        if res is not None:
            px, slip = res
            ev.fill_price = ctx['slip'](order, px) if slip else px
            ev.fill_quantity = order.quantity - order.quantity_filled
            ev.status = OrderStatus.FILLED
        return ev

    def market_fill(self, asset, order):
        return self._engine_event(asset, order)

    def limit_fill(self, asset, order):
        return self._engine_event(asset, order)

    def stop_market_fill(self, asset, order):
        return self._engine_event(asset, order)

    def stop_limit_fill(self, asset, order):
        return self._engine_event(asset, order)

    def market_on_open_fill(self, asset, order):
        return self._engine_event(asset, order)

    def market_on_close_fill(self, asset, order):
        return self._engine_event(asset, order)

    def limit_if_touched_fill(self, asset, order):
        return self._engine_event(asset, order)

    def trailing_stop_fill(self, asset, order):
        return self._engine_event(asset, order)

    def combo_market_fill(self, order, parameters):
        return [self._engine_event(parameters.security, order)]

    def combo_limit_fill(self, order, parameters):
        return [self._engine_event(parameters.security, order)]

    def combo_leg_limit_fill(self, order, parameters):
        return [self._engine_event(parameters.security, order)]

    _BY_TYPE = {OrderType.MARKET: 'market_fill', OrderType.LIMIT: 'limit_fill',
                OrderType.STOP_MARKET: 'stop_market_fill', OrderType.STOP_LIMIT: 'stop_limit_fill',
                OrderType.MARKET_ON_OPEN: 'market_on_open_fill', OrderType.MARKET_ON_CLOSE: 'market_on_close_fill',
                OrderType.LIMIT_IF_TOUCHED: 'limit_if_touched_fill', OrderType.TRAILING_STOP: 'trailing_stop_fill'}

    def fill(self, parameters):
        order, asset = parameters.order, parameters.security
        name = self._BY_TYPE.get(order.order_type)
        if name is None:
            return Fill([])
        ev = getattr(self, name)(asset, order)
        return Fill(ev if isinstance(ev, (list, tuple)) else [ev])

    @staticmethod
    def get_prices(asset, direction=None):
        last = asset.get_last_data()
        from types import SimpleNamespace
        if last is None:
            p = float(asset.price or 0.0)
            return SimpleNamespace(open=p, high=p, low=p, close=p, current=p, end_time=None)
        return SimpleNamespace(open=last.open, high=last.high, low=last.low, close=last.close, current=last.close,
                               end_time=getattr(last, 'end_time', None))

    MarketFill = market_fill
    LimitFill = limit_fill
    StopMarketFill = stop_market_fill
    Fill_ = fill


class ImmediateFillModel(FillModel):
    pass


class EquityFillModel(FillModel):
    pass


class FutureFillModel(FillModel):
    pass


class FutureOptionFillModel(FillModel):
    pass


# ---------------------------------------------------------------- buying power

class BuyingPowerModel:
    """Base buying-power model: the engine's margin rules (initial margin = |value| / leverage,
    orders that would push margin above equity are rejected). Subclasses override
    has_sufficient_buying_power_for_order and/or get_maximum_order_quantity_for_target_buying_power."""

    def __init__(self, leverage=None, required_free_buying_power_percent=0.0):
        self._leverage = leverage
        self.required_free_buying_power_percent = required_free_buying_power_percent

    def get_leverage(self, security):
        return float(self._leverage if self._leverage else getattr(security, 'leverage', 1.0) or 1.0)

    def set_leverage(self, security, leverage):
        self._leverage = leverage
        security.leverage = leverage

    def has_sufficient_buying_power_for_order(self, parameters):
        broker = getattr(getattr(parameters.portfolio, '_algo', None), '_broker', None)
        if broker is None:
            return HasSufficientBuyingPowerForOrderResult(True)
        reason = broker._engine_buying_power_shortfall(str(parameters.security.symbol).upper(), parameters.order)
        return HasSufficientBuyingPowerForOrderResult(reason is None, reason or '')

    def get_maximum_order_quantity_for_target_buying_power(self, parameters):
        algo = getattr(parameters.portfolio, '_algo', None)
        if algo is None:
            return GetMaximumOrderQuantityResult(0, 'no algorithm')
        tpv = parameters.portfolio.total_portfolio_value
        target = parameters.target_buying_power
        q = algo._engine_order_quantity(parameters.security.symbol, target)
        return GetMaximumOrderQuantityResult(q)

    def get_maximum_order_quantity_for_delta_buying_power(self, parameters):
        return GetMaximumOrderQuantityResult(0)

    def get_buying_power(self, parameters):
        pf = getattr(parameters, 'portfolio', None)
        return BuyingPower(pf.margin_remaining if pf is not None else 0.0)

    def get_maintenance_margin(self, parameters):
        sec = getattr(parameters, 'security', parameters)
        h = sec.holdings
        return MaintenanceMargin(abs(h.holdings_value) / self.get_leverage(sec))

    def get_initial_margin_requirement(self, parameters):
        sec = parameters.security
        q = float(getattr(parameters, 'quantity', 0.0))
        return InitialMargin(abs(q * float(sec.price or 0.0)) / self.get_leverage(sec))

    def get_reserved_buying_power_for_position(self, parameters):
        return parameters.result_in_account_currency(self.get_maintenance_margin(parameters.security).value)

    # margin requirement per unit of value; None = engine default (1 / leverage)
    def margin_fraction(self, security):
        return 1.0 / self.get_leverage(security)

    HasSufficientBuyingPowerForOrder = has_sufficient_buying_power_for_order
    GetMaximumOrderQuantityForTargetBuyingPower = get_maximum_order_quantity_for_target_buying_power


class SecurityMarginModel(BuyingPowerModel):
    def __init__(self, leverage=1.0, required_free_buying_power_percent=0.0):
        super().__init__(leverage, required_free_buying_power_percent)


class PatternDayTradingMarginModel(SecurityMarginModel):
    def __init__(self, closed_market_leverage=2.0, open_market_leverage=4.0):
        super().__init__(closed_market_leverage)


class CashBuyingPowerModel(BuyingPowerModel):
    def __init__(self):
        super().__init__(1.0)


class ConstantBuyingPowerModel(BuyingPowerModel):
    """Margin = |value| x a constant requirement (1 = fully funded)."""
    def __init__(self, margin_requirement=1.0):
        super().__init__(1.0 / float(margin_requirement) if margin_requirement else 1e9)
        self._req = float(margin_requirement)

    def margin_fraction(self, security):
        return self._req


class NullBuyingPowerModel(BuyingPowerModel):
    """No margin requirement: every order has enough buying power."""
    def has_sufficient_buying_power_for_order(self, parameters):
        return HasSufficientBuyingPowerForOrderResult(True)

    def margin_fraction(self, security):
        return 0.0


BuyingPowerModel.NULL = NullBuyingPowerModel()
BuyingPowerModel.Null = BuyingPowerModel.NULL
SecurityMarginModel.NULL = BuyingPowerModel.NULL
SecurityMarginModel.Null = BuyingPowerModel.NULL


def is_overridden(obj, base, name) -> bool:
    """True when obj's class defines `name` itself (a strategy's own model method)."""
    fn = getattr(type(obj), name, None)
    return fn is not None and fn is not getattr(base, name, None)


# ---------------------------------------------------------------- margin calls / interest / settlement

class MarginCallModel:
    def get_margin_call_orders(self, *args):
        return []

    def execute_margin_call(self, *args):
        return []


class DefaultMarginCallModel(MarginCallModel):
    def __init__(self, portfolio=None, default_order_properties=None, margin_buffer=0.1):
        self.portfolio = portfolio
        self.default_order_properties = default_order_properties
        self.margin_buffer = margin_buffer


class _NullMarginCallModel(MarginCallModel):
    """No margin calls."""
    is_null = True


MarginCallModel.NULL = _NullMarginCallModel()
MarginCallModel.Null = MarginCallModel.NULL


class MarginInterestRateModel:
    def apply_margin_interest_rate(self, parameters):
        pass


MarginInterestRateModel.NULL = MarginInterestRateModel()
MarginInterestRateModel.Null = MarginInterestRateModel.NULL


class ImmediateSettlementModel:
    def apply_funds(self, parameters):
        pass

    def scan(self, parameters):
        pass

    def get_unsettled_cash(self):
        return None


class DelayedSettlementModel(ImmediateSettlementModel):
    """Sale proceeds settle `days` trading days later (cash accounts)."""
    def __init__(self, number_of_days=3, time_of_day=None):
        self.number_of_days = int(number_of_days)
        self.time_of_day = time_of_day or timedelta(hours=8)


# ---------------------------------------------------------------- brokerage messages

class BrokerageMessageType:
    INFORMATION = 0
    WARNING = 1
    ERROR = 2
    RECONNECT = 3
    DISCONNECT = 4
    Information = 0
    Warning = 1
    Error = 2
    Reconnect = 3
    Disconnect = 4
    _NAMES = {0: 'Information', 1: 'Warning', 2: 'Error', 3: 'Reconnect', 4: 'Disconnect'}


class BrokerageMessageEvent:
    def __init__(self, type_=0, code="", message=""):
        self.type = type_
        self.code = code
        self.message = message

    def __str__(self):
        return f"{BrokerageMessageType._NAMES.get(int(self.type), self.type)} - Code: {self.code} - {self.message}"

    Message = property(lambda self: self.message)
