# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-EFDED6A6
# Category: Regression Test
# Description: Tests that orders are denied if they exceed the max shortable quantity
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class RegressionTestShortableProvider(LocalDiskShortableProvider):
    def __init__(self):
        super().__init__("testbrokerage")

### <summary>
### Tests that orders are denied if they exceed the max shortable quantity.
### </summary>
class ShortableProviderOrdersRejectedRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.orders_allowed = []
        self.orders_denied = []
        self.initialized = False
        self.invalidated_allowed_order = False
        self.invalidated_new_order_with_portfolio_holdings = False

        self.set_start_date(2013, 10, 4)
        self.set_end_date(2013, 10, 11)
        self.set_cash(10000000)

        self.spy = self.add_equity("SPY", Resolution.MINUTE)
        self.aig = self.add_equity("AIG", Resolution.MINUTE)

        self.spy.set_shortable_provider(RegressionTestShortableProvider())
        self.aig.set_shortable_provider(RegressionTestShortableProvider())

    def on_data(self, data):
        if not self.initialized:
            self.handle_order(self.limit_order(self.spy.symbol, -1001, 10000)) # Should be canceled, exceeds the max shortable quantity
            order_ticket = self.limit_order(self.spy.symbol, -1000, 10000)
            self.handle_order(order_ticket) # Allowed, orders at or below 1000 should be accepted
            self.handle_order(self.limit_order(self.spy.symbol, -10, 0.01)) # Should be canceled, the total quantity we would be short would exceed the max shortable quantity.

            response = order_ticket.update_quantity(-999) # should be allowed, we are reducing the quantity we want to short
            if not response.is_success:
                raise ValueError("Order update should of succeeded!");

            self.initialized = True
            return

        if not self.invalidated_allowed_order:
            if len(self.orders_allowed) != 1:
                raise Exception(f"Expected 1 successful order, found: {len(self.orders_allowed)}")
            if len(self.orders_denied) != 2:
                raise Exception(f"Expected 2 failed orders, found: {len(self.orders_denied)}")

            allowed_order = self.orders_allowed[0]
            order_update = UpdateOrderFields()
            order_update.limit_price = 0.01
            order_update.quantity = -1001
            order_update.tag = "Testing updating and exceeding maximum quantity"

            response = allowed_order.update(order_update)
            if response.error_code != OrderResponseErrorCode.EXCEEDS_SHORTABLE_QUANTITY:
                raise Exception(f"Expected order to fail due to exceeded shortable quantity, found: {response.error_code}")

            cancel_response = allowed_order.cancel()
            if cancel_response.is_error:
                raise Exception("Expected to be able to cancel open order after bad qty update")

            self.invalidated_allowed_order = True
            self.orders_denied.clear()
            self.orders_allowed.clear()
            return

        if not self.invalidated_new_order_with_portfolio_holdings:
            self.handle_order(self.market_order(self.spy.symbol, -1000)) # Should succeed, no holdings and no open orders to stop this
            spy_shares = self.portfolio[self.spy.symbol].quantity
            if spy_shares != -1000:
                raise Exception(f"Expected -1000 shares in portfolio, found: {spy_shares}")

            self.handle_order(self.limit_order(self.spy.symbol, -1, 0.01)) # Should fail, portfolio holdings are at the max shortable quantity.
            if len(self.orders_denied) != 1:
                raise Exception(f"Expected limit order to fail due to existing holdings, but found {len(self.orders_denied)} failures")

            self.orders_allowed.clear()
            self.orders_denied.clear()

            self.handle_order(self.market_order(self.aig.symbol, -1001))
            if len(self.orders_allowed) != 1:
                raise Exception(f"Expected market order of -1001 BAC to not fail")

            self.invalidated_new_order_with_portfolio_holdings = True

    def handle_order(self, order_ticket):
        if order_ticket.submit_request.status == OrderRequestStatus.ERROR:
            self.orders_denied.append(order_ticket)
            return

        self.orders_allowed.append(order_ticket)
