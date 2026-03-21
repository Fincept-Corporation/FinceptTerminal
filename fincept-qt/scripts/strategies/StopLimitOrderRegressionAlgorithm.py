# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-58C9B30E
# Category: Regression Test
# Description: Basic algorithm demonstrating how to place stop limit orders
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Basic algorithm demonstrating how to place stop limit orders.
### </summary>
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="placing orders" />
### <meta name="tag" content="stop limit order"/>
class StopLimitOrderRegressionAlgorithm(QCAlgorithm):
    '''Basic algorithm demonstrating how to place stop limit orders.'''

    tolerance = 0.001
    fast_period = 30
    slow_period = 60

    def initialize(self):
        self.set_start_date(2013, 1, 1)
        self.set_end_date(2017, 1, 1)
        self.set_cash(100000)

        self._symbol = self.add_equity("SPY", Resolution.DAILY).symbol

        self._fast = self.ema(self._symbol, self.fast_period, Resolution.DAILY)
        self._slow = self.ema(self._symbol, self.slow_period, Resolution.DAILY)

        self._buy_order_ticket: OrderTicket = None
        self._sell_order_ticket: OrderTicket = None
        self._previous_slice: Slice = None

    def on_data(self, slice: Slice):
        if not self.is_ready():
            return

        security = self.securities[self._symbol]
        if self._buy_order_ticket is None and self.trend_is_up():
            self._buy_order_ticket = self.stop_limit_order(self._symbol, 100, stop_price=security.high * 1.10, limit_price=security.high * 1.11)
        elif self._buy_order_ticket.status == OrderStatus.FILLED and self._sell_order_ticket is None and self.trend_is_down():
            self._sell_order_ticket = self.stop_limit_order(self._symbol, -100, stop_price=security.low * 0.99, limit_price=security.low * 0.98)

    def on_order_event(self, order_event: OrderEvent):
        if order_event.status == OrderStatus.FILLED:
            order: StopLimitOrder = self.transactions.get_order_by_id(order_event.order_id)
            if not order.stop_triggered:
                raise Exception("StopLimitOrder StopTriggered should haven been set if the order filled.")

            if order_event.direction == OrderDirection.BUY:
                limit_price = self._buy_order_ticket.get(OrderField.LIMIT_PRICE)
                if order_event.fill_price > limit_price:
                    raise Exception(f"Buy stop limit order should have filled with price less than or equal to the limit price {limit_price}. "
                                    f"Fill price: {order_event.fill_price}")
            else:
                limit_price = self._sell_order_ticket.get(OrderField.LIMIT_PRICE)
                if order_event.fill_price < limit_price:
                    raise Exception(f"Sell stop limit order should have filled with price greater than or equal to the limit price {limit_price}. "
                                    f"Fill price: {order_event.fill_price}")

    def is_ready(self):
        return self._fast.is_ready and self._slow.is_ready

    def trend_is_up(self):
        return self.is_ready() and self._fast.current.value > self._slow.current.value * (1 + self.tolerance)

    def trend_is_down(self):
        return self.is_ready() and self._fast.current.value < self._slow.current.value * (1 + self.tolerance)
