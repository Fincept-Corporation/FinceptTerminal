# ============================================================================
# Fincept Terminal - Strategy Engine: LEAN order requests and responses
#
# Every order goes through a request: SubmitOrderRequest (placing), UpdateOrderRequest
# (ticket.update / update_quantity / update_limit_price ...), CancelOrderRequest
# (ticket.cancel). Each request gets an OrderResponse (is_success / error_code /
# error_message) and a status (OrderRequestStatus), as in LEAN. The ticket keeps the
# submit request, every update request and the cancel request.
# ============================================================================

from datetime import datetime, timedelta
from typing import Optional

from .enums import OrderRequestStatus, OrderType, SecurityType


class OrderResponseErrorCode:
    NONE = 0
    PROCESSING_ERROR = -1
    ORDER_ALREADY_EXISTS = -2
    INSUFFICIENT_BUYING_POWER = -3
    BROKERAGE_MODEL_REFUSED_TO_SUBMIT_ORDER = -4
    BROKERAGE_FAILED_TO_SUBMIT_ORDER = -5
    BROKERAGE_FAILED_TO_UPDATE_ORDER = -6
    BROKERAGE_HANDLER_REFUSED_TO_UPDATE_ORDER = -7
    BROKERAGE_FAILED_TO_CANCEL_ORDER = -8
    INVALID_ORDER_STATUS = -9
    UNABLE_TO_FIND_ORDER = -10
    ORDER_QUANTITY_ZERO = -11
    UNSUPPORTED_REQUEST_TYPE = -12
    PRE_ORDER_CHECKS_ERROR = -13
    MISSING_SECURITY = -14
    EXCHANGE_NOT_OPEN = -15
    SECURITY_PRICE_ZERO = -16
    FOREX_BASE_AND_QUOTE_CURRENCIES_REQUIRED = -17
    FOREX_CONVERSION_RATE_ZERO = -18
    SECURITY_HAS_NO_DATA = -19
    EXCEEDED_MAXIMUM_ORDERS = -20
    MARKET_ON_CLOSE_ORDER_TOO_LATE = -21
    INVALID_REQUEST = -22
    REQUEST_CANCELED = -23
    ALGORITHM_WARMING_UP = -24
    BROKERAGE_MODEL_REFUSED_TO_UPDATE_ORDER = -25
    QUOTE_CURRENCY_REQUIRED = -26
    CONVERSION_RATE_ZERO = -27
    NON_TRADABLE_SECURITY = -28
    NON_EXERCISABLE_SECURITY = -29
    ORDER_QUANTITY_LESS_THAN_LOT_SIZE = -30
    EXCEEDS_SHORTABLE_QUANTITY = -31
    INVALID_NEW_ORDER_STATUS = -32
    EUROPEAN_OPTION_NOT_EXPIRED_ON_EXERCISE = -33
    OPTION_ORDER_ON_STOCK_SPLIT = -34
    MARKET_ON_OPEN_NOT_ALLOWED_DURING_REGULAR_HOURS = -35
    # PascalCase
    None_ = 0
    ProcessingError = -1
    InsufficientBuyingPower = -3
    BrokerageModelRefusedToSubmitOrder = -4
    InvalidOrderStatus = -9
    UnableToFindOrder = -10
    OrderQuantityZero = -11
    MissingSecurity = -14
    MarketOnCloseOrderTooLate = -21
    AlgorithmWarmingUp = -24
    BrokerageModelRefusedToUpdateOrder = -25
    ExceedsShortableQuantity = -31


class OrderResponse:
    """Result of an order request."""

    def __init__(self, order_id: int = 0, error_code: int = 0, error_message: str = ''):
        self.order_id = order_id
        self.error_code = error_code
        self.error_message = error_message or ''
        self.is_processed = True

    @property
    def is_success(self) -> bool:
        return self.error_code == 0

    @property
    def is_error(self) -> bool:
        return self.error_code != 0

    @staticmethod
    def success(request=None):
        return OrderResponse(getattr(request, 'order_id', 0), 0, '')

    @staticmethod
    def error(request, code: int, message: str):
        return OrderResponse(getattr(request, 'order_id', 0), code, message)

    def __str__(self):
        return 'OrderResponse: Success' if self.is_success else \
            f'OrderResponse: Error: {self.error_code}: {self.error_message}'

    __repr__ = __str__
    IsSuccess = property(lambda self: self.is_success)
    IsError = property(lambda self: self.is_error)
    ErrorMessage = property(lambda self: self.error_message)
    ErrorCode = property(lambda self: self.error_code)


OrderResponse.UNPROCESSED = OrderResponse(0, 0, '')
OrderResponse.UNPROCESSED.is_processed = False


class OrderRequest:
    def __init__(self, time=None, order_id: int = 0, tag: str = ''):
        self.time = time or datetime.now()
        self.order_id = order_id
        self.tag = tag or ''
        self.status = OrderRequestStatus.UNPROCESSED
        self.response = OrderResponse.UNPROCESSED

    def set_response(self, response: OrderResponse, status=None):
        self.response = response
        self.status = status if status is not None else (
            OrderRequestStatus.PROCESSED if response.is_success else OrderRequestStatus.ERROR)

    def to_string(self):
        return str(self)


class SubmitOrderRequest(OrderRequest):
    """LEAN SubmitOrderRequest. Accepted positional forms:
        (order_type, security_type, symbol, quantity, stop_price, limit_price, time, tag[, properties])
        (order_type, security_type, symbol, quantity, stop_price, limit_price, trigger_price, time, tag[, properties])
        (order_type, security_type, symbol, quantity, stop_price, limit_price, trigger_price,
         trailing_amount, trailing_as_percentage, time, tag[, properties])"""

    def __init__(self, order_type, security_type=None, symbol=None, quantity=0.0, stop_price=0.0,
                 limit_price=0.0, *args, **kwargs):
        rest = list(args)
        trigger = kwargs.get('trigger_price', 0.0)
        trailing_amount = kwargs.get('trailing_amount', 0.0)
        as_pct = kwargs.get('trailing_as_percentage', False)
        time = kwargs.get('time')
        tag = kwargs.get('tag', '')
        props = kwargs.get('properties', kwargs.get('order_properties'))
        # numbers before the time are trigger / trailing amount / as-percentage
        nums = []
        while rest and not isinstance(rest[0], datetime) and not isinstance(rest[0], str):
            nums.append(rest.pop(0))
        if len(nums) >= 1:
            trigger = nums[0] or 0.0
        if len(nums) >= 2:
            trailing_amount = nums[1] or 0.0
        if len(nums) >= 3:
            as_pct = bool(nums[2])
        if rest and isinstance(rest[0], datetime):
            time = rest.pop(0)
        if rest and isinstance(rest[0], str):
            tag = rest.pop(0)
        if rest:
            props = rest.pop(0)
        super().__init__(time, 0, tag)
        self.order_type = order_type
        self.security_type = security_type if security_type is not None else SecurityType.EQUITY
        self.symbol = symbol
        self.quantity = float(quantity or 0.0)
        self.stop_price = float(stop_price or 0.0)
        self.limit_price = float(limit_price or 0.0)
        self.trigger_price = float(trigger or 0.0)
        self.trailing_amount = float(trailing_amount or 0.0)
        self.trailing_as_percentage = bool(as_pct)
        self.order_properties = props
        self.group_order_manager = kwargs.get('group_order_manager')

    def __str__(self):
        return (f"{self.time} UTC: Submit Order: ({self.order_id}) - {getattr(self.order_type, 'name', self.order_type)} "
                f"{self.quantity:g} {self.symbol} Status: {getattr(self.status, 'name', self.status)}")


class UpdateOrderRequest(OrderRequest):
    def __init__(self, time=None, order_id: int = 0, fields=None):
        super().__init__(time, order_id, getattr(fields, 'tag', None) or '')
        self.quantity = getattr(fields, 'quantity', None)
        self.limit_price = getattr(fields, 'limit_price', None)
        self.stop_price = getattr(fields, 'stop_price', None)
        self.trigger_price = getattr(fields, 'trigger_price', None)
        self.trailing_amount = getattr(fields, 'trailing_amount', None)
        self.tag = getattr(fields, 'tag', None)

    def is_allowed_for_closed_order(self) -> bool:
        return (self.quantity is None and self.limit_price is None and self.stop_price is None
                and self.trigger_price is None and self.trailing_amount is None)

    def __str__(self):
        parts = []
        for k in ('quantity', 'limit_price', 'stop_price', 'trigger_price', 'trailing_amount', 'tag'):
            v = getattr(self, k)
            if v is not None:
                parts.append(f"{k}: {v}")
        return f"{self.time} UTC: Update Order: ({self.order_id}) - {', '.join(parts)} Status: {self.status.name}"


class CancelOrderRequest(OrderRequest):
    def __init__(self, time=None, order_id: int = 0, tag: str = ''):
        super().__init__(time, order_id, tag)

    def __str__(self):
        return f"{self.time} UTC: Cancel Order: ({self.order_id}) - {self.tag} Status: {self.status.name}"


class Order:
    """LEAN Order. transactions.get_order_by_id / get_orders return one of the typed subclasses
    (MarketOrder, LimitOrder, ...) as a live view of the order's ticket; constructed directly it is
    a plain order description (symbol, quantity, time, tag)."""

    _ORDER_TYPE = OrderType.MARKET

    def __init__(self, symbol=None, quantity=0.0, time=None, tag="", properties=None, *args, **kwargs):
        object.__setattr__(self, '_ticket', None)
        self.symbol = symbol
        self.quantity = float(quantity or 0.0)
        self.time = time
        self.tag = tag or ''
        self.properties = properties
        self.id = 0
        self.status = None
        for k, v in kwargs.items():
            setattr(self, k, v)

    @staticmethod
    def view(ticket):
        cls = _ORDER_CLASSES.get(ticket.order_type, Order)
        o = cls.__new__(cls)
        object.__setattr__(o, '_ticket', ticket)
        return o

    def __getattr__(self, name):
        t = self.__dict__.get('_ticket')
        if t is None or name.startswith('__'):
            raise AttributeError(name)
        return getattr(t, name)

    def __setattr__(self, name, value):
        t = self.__dict__.get('_ticket')
        if t is not None and hasattr(t, name):
            setattr(t, name, value)
        else:
            object.__setattr__(self, name, value)

    @property
    def type(self):
        t = self.__dict__.get('_ticket')
        return t.order_type if t is not None else self._ORDER_TYPE

    @property
    def direction(self):
        q = self.quantity
        return 0 if q > 0 else (1 if q < 0 else 2)

    @property
    def absolute_quantity(self):
        return abs(self.quantity)

    def __eq__(self, other):
        a = self.__dict__.get('_ticket')
        b = other.__dict__.get('_ticket') if isinstance(other, Order) else other
        return a is not None and a is b

    def __hash__(self):
        return id(self.__dict__.get('_ticket') or self)

    def __str__(self):
        t = self.__dict__.get('_ticket')
        if t is None:
            return f"{getattr(self._ORDER_TYPE, 'name', '')} order for {self.quantity:g} unit(s) of {self.symbol}"
        s = (f"OrderId: {t.order_id} (BrokerId: {t.order_id}) {t.status.name} {t.order_type.name} order for "
             f"{t.quantity:g} unit(s) of {t.symbol}")
        if t.limit_price:
            s += f" at limit {t.limit_price:g}"
        if t.stop_price:
            s += f" with stop {t.stop_price:g}"
        if t.tag:
            s += f" Tag: {t.tag}"
        return s

    __repr__ = __str__

    def to_string(self):
        return str(self)

    Id = property(lambda self: self.id)
    Type = property(lambda self: self.type)
    AbsoluteQuantity = property(lambda self: self.absolute_quantity)
    Quantity = property(lambda self: self.quantity)
    Symbol = property(lambda self: self.symbol)
    Status = property(lambda self: self.status)
    Direction = property(lambda self: self.direction)
    Tag = property(lambda self: self.tag)
    Price = property(lambda self: self.price)
    Time = property(lambda self: self.time)
    LimitPrice = property(lambda self: self.limit_price)
    StopPrice = property(lambda self: self.stop_price)


class MarketOrder(Order):
    _ORDER_TYPE = OrderType.MARKET


class LimitOrder(Order):
    _ORDER_TYPE = OrderType.LIMIT


class StopMarketOrder(Order):
    _ORDER_TYPE = OrderType.STOP_MARKET


class StopLimitOrder(Order):
    _ORDER_TYPE = OrderType.STOP_LIMIT


class LimitIfTouchedOrder(Order):
    _ORDER_TYPE = OrderType.LIMIT_IF_TOUCHED


class TrailingStopOrder(Order):
    _ORDER_TYPE = OrderType.TRAILING_STOP

    @staticmethod
    def calculate_stop_price(current_market_price, trailing_amount, trailing_as_percentage, direction):
        sell = int(direction) == 1
        if trailing_as_percentage:
            return current_market_price * (1 - trailing_amount) if sell else current_market_price * (1 + trailing_amount)
        return current_market_price - trailing_amount if sell else current_market_price + trailing_amount

    CalculateStopPrice = calculate_stop_price


class MarketOnOpenOrder(Order):
    _ORDER_TYPE = OrderType.MARKET_ON_OPEN


class MarketOnCloseOrder(Order):
    """LEAN MarketOnCloseOrder: an MOC order must reach the exchange before the close minus this buffer."""
    _ORDER_TYPE = OrderType.MARKET_ON_CLOSE
    DEFAULT_SUBMISSION_TIME_BUFFER = timedelta(minutes=15.5)
    submission_time_buffer = DEFAULT_SUBMISSION_TIME_BUFFER
    DefaultSubmissionTimeBuffer = DEFAULT_SUBMISSION_TIME_BUFFER


class ComboMarketOrder(Order):
    _ORDER_TYPE = OrderType.COMBO_MARKET


class ComboLimitOrder(Order):
    _ORDER_TYPE = OrderType.COMBO_LIMIT


class ComboLegLimitOrder(Order):
    _ORDER_TYPE = OrderType.COMBO_LEG_LIMIT


class OptionExerciseOrder(Order):
    _ORDER_TYPE = None


_ORDER_CLASSES = {OrderType.MARKET: MarketOrder, OrderType.LIMIT: LimitOrder,
                  OrderType.STOP_MARKET: StopMarketOrder, OrderType.STOP_LIMIT: StopLimitOrder,
                  OrderType.LIMIT_IF_TOUCHED: LimitIfTouchedOrder, OrderType.TRAILING_STOP: TrailingStopOrder,
                  OrderType.MARKET_ON_OPEN: MarketOnOpenOrder, OrderType.MARKET_ON_CLOSE: MarketOnCloseOrder,
                  OrderType.COMBO_MARKET: ComboMarketOrder, OrderType.COMBO_LIMIT: ComboLimitOrder,
                  OrderType.COMBO_LEG_LIMIT: ComboLegLimitOrder}

# LEAN pre-order checks: a request failing one never becomes an order (no order id, no event)
PRE_ORDER_CHECK_CODES = {-11, -14, -16, -19, -24, -31, -21, -15, -28, -29}
