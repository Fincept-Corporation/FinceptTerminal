# Order tickets, requests and order events through a whole (offline) backtest.
from datetime import datetime

from AlgorithmImports import *  # noqa: F401,F403
from engine_fixture import Feed, run, ok
from fincept_engine.orders import OrderResponseErrorCode

BASE = datetime(2021, 12, 1)


def spy_bar(d):
    """Open = 100 + days since BASE, close = open + 0.5, range +-1 around them."""
    o = 100.0 + (d - BASE).days
    return (o, o + 1.5, o - 1.0, o + 0.5)


def feed():
    return Feed({'SPY': spy_bar})


class _Base(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2022, 1, 3)
        self.set_end_date(2022, 1, 14)
        self.set_cash(100000)
        self.spy = self.add_equity('SPY', Resolution.DAILY).symbol
        self.events = []
        type(self).last = self

    def on_order_event(self, e):
        self.events.append((e.order_id, e.status, e.fill_quantity, e.fill_price, self.time))


class MarketTicket(_Base):
    ticket = None

    def on_data(self, data):
        if self.ticket is None:
            self.ticket = self.market_order(self.spy, 10)
            self.status_at_submit = self.ticket.status


def test_market_order_fills_at_next_open_with_submitted_then_filled_events():
    d = ok(run(MarketTicket, '2022-01-03', '2022-01-14', feed()))
    a = MarketTicket.last
    tk = a.ticket
    assert a.status_at_submit == OrderStatus.SUBMITTED
    assert tk.status == OrderStatus.FILLED
    assert tk.quantity_filled == 10
    jan4_open = spy_bar(datetime(2022, 1, 4))[0]
    assert tk.average_fill_price == jan4_open
    statuses = [e[1] for e in a.events]
    assert statuses == [OrderStatus.SUBMITTED, OrderStatus.FILLED]
    assert a.events[1][2] == 10 and a.events[1][3] == jan4_open
    assert a.portfolio[a.spy].quantity == 10
    assert d['orders_filled'] == 1


class LimitUpdateCancel(_Base):
    t1 = t2 = None
    step = 0

    def on_data(self, data):
        px = self.securities[self.spy].price
        if self.step == 0:
            self.t1 = self.limit_order(self.spy, 5, px - 50)      # far below the market
            self.t2 = self.limit_order(self.spy, 5, px - 50)
        elif self.step == 1:
            self.r_update = self.t1.update_limit_price(px + 5)     # now marketable: fills at the next open
            self.r_cancel = self.t2.cancel()
        elif self.step == 2:
            self.r_after = self.t2.update_quantity(7)              # the order is closed
            self.open_after = list(self.transactions.get_open_orders(self.spy))
        self.step += 1


def test_limit_order_update_and_cancel():
    ok(run(LimitUpdateCancel, '2022-01-03', '2022-01-14', feed()))
    a = LimitUpdateCancel.last
    assert a.r_update.is_success and a.r_cancel.is_success
    assert a.t1.status == OrderStatus.FILLED
    jan5_open = spy_bar(datetime(2022, 1, 5))[0]
    assert a.t1.average_fill_price == jan5_open                    # limit above the open: fills at the open
    assert a.t2.status == OrderStatus.CANCELED
    assert a.r_after.is_error
    assert a.r_after.error_code == OrderResponseErrorCode.INVALID_ORDER_STATUS
    assert a.open_after == []
    t2_statuses = [e[1] for e in a.events if e[0] == a.t2.order_id]
    assert t2_statuses == [OrderStatus.SUBMITTED, OrderStatus.CANCELED]


class ZeroQuantity(_Base):
    tk = None

    def on_data(self, data):
        if self.tk is None:
            self.tk = self.market_order(self.spy, 0)


def test_zero_quantity_order_is_invalid_and_never_fills():
    d = ok(run(ZeroQuantity, '2022-01-03', '2022-01-07', feed()))
    a = ZeroQuantity.last
    assert a.tk.status == OrderStatus.INVALID
    assert a.tk.submit_request.response.error_code == OrderResponseErrorCode.ORDER_QUANTITY_ZERO
    assert a.portfolio[a.spy].quantity == 0
    assert d['orders_filled'] == 0


class ViaSubmitRequest(_Base):
    tk = None

    def on_data(self, data):
        if self.tk is None:
            req = SubmitOrderRequest(OrderType.MARKET, SecurityType.EQUITY, self.spy, 3, 0, 0, self.utc_time,
                                     "via request")
            self.tk = self.transactions.add_order(req)


def test_submit_order_request_through_transactions():
    ok(run(ViaSubmitRequest, '2022-01-03', '2022-01-07', feed()))
    a = ViaSubmitRequest.last
    assert a.tk.status == OrderStatus.FILLED
    assert a.tk.tag == "via request"
    order = a.transactions.get_order_by_id(a.tk.order_id)
    assert order.type == OrderType.MARKET and order.quantity == 3
    assert a.portfolio[a.spy].quantity == 3


class StopMarket(_Base):
    tk = None

    def on_data(self, data):
        if self.tk is None:
            self.market_order(self.spy, 10)
            # sell stop below the market: the rising series never reaches it
            self.tk = self.stop_market_order(self.spy, -10, self.securities[self.spy].price - 20)


def test_untouched_stop_stays_open_until_the_end():
    d = ok(run(StopMarket, '2022-01-03', '2022-01-14', feed()))
    a = StopMarket.last
    assert a.tk.status == OrderStatus.SUBMITTED
    assert d['unfilled_orders'] == 1
