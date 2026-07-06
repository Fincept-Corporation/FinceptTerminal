# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-78D0ECA0
# Category: Regression Test
# Description: Basic algorithm demonstrating how to place LimitIfTouched orders
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from collections import deque

### <summary>
### Basic algorithm demonstrating how to place LimitIfTouched orders.
### </summary>
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="placing orders" />`
### <meta name="tag" content="limit if touched order"/>
class LimitIfTouchedRegressionAlgorithm(QCAlgorithm):
    _expected_events = deque([
        "Time: 10/10/2013 13:31:00 OrderID: 72 EventID: 399 Symbol: SPY Status: Filled Quantity: -1 FillQuantity: -1 FillPrice: $144.6434 LimitPrice: $144.3551 TriggerPrice: $143.61 OrderFee: 1 USD",
        "Time: 10/10/2013 15:57:00 OrderID: 73 EventID: 156 Symbol: SPY Status: Filled Quantity: -1 FillQuantity: -1 FillPrice: $145.6636 LimitPrice: $145.6434 TriggerPrice: $144.89 OrderFee: 1 USD",
        "Time: 10/11/2013 15:37:00 OrderID: 74 EventID: 380 Symbol: SPY Status: Filled Quantity: -1 FillQuantity: -1 FillPrice: $146.7185 LimitPrice: $146.6723 TriggerPrice: $145.92 OrderFee: 1 USD"    ])

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)
        self.add_equity("SPY")

    def on_data(self, data):
        if data.contains_key("SPY"):
            if len(self.transactions.get_open_orders()) == 0:
                self._negative = 1 if self.time.day < 9 else -1
                order_request = SubmitOrderRequest(OrderType.LIMIT_IF_TOUCHED, SecurityType.EQUITY, "SPY",
                                                  self._negative * 10, 0,
                                                  data["SPY"].price - self._negative,
                                                  data["SPY"].price - 0.25 * self._negative, self.utc_time,
                                                  f"LIT - Quantity: {self._negative * 10}")
                self._request = self.transactions.add_order(order_request)
                return

            if self._request is not None:
                if self._request.quantity == 1:
                    self.transactions.cancel_open_orders()
                    self._request = None
                    return

                new_quantity = int(self._request.quantity - self._negative)
                self._request.update_quantity(new_quantity, f"LIT - Quantity: {new_quantity}")
                self._request.update_trigger_price(Extensions.round_to_significant_digits(self._request.get(OrderField.TRIGGER_PRICE), 5))

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            expected = self._expected_events.popleft()
            if str(order_event) != expected:
                raise Exception(f"order_event {order_event.id} differed from {expected}. Actual {order_event}")
