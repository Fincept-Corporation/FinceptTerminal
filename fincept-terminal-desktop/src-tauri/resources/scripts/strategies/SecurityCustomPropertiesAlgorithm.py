# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-B0B2E65B
# Category: General Strategy
# Description: Demonstration of how to use custom security properties. In this algorithm we trade a security based on the values of ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from cmath import isclose
from AlgorithmImports import *

### <summary>
### Demonstration of how to use custom security properties.
### In this algorithm we trade a security based on the values of a slow and fast EMAs which are stored in the security itself.
### </summary>
class SecurityCustomPropertiesAlgorithm(QCAlgorithm):
    '''Demonstration of how to use custom security properties.
    In this algorithm we trade a security based on the values of a slow and fast EMAs which are stored in the security itself.'''

    def initialize(self):
        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10,11)
        self.set_cash(100000)

        self.spy = self.add_equity("SPY", Resolution.MINUTE)

        # Using the dynamic interface to store our indicator as a custom property.
        self.spy.slow_ema = self.ema(self.spy.symbol, 30, Resolution.MINUTE)

        # Using the generic interface to store our indicator as a custom property.
        self.spy.add("fast_ema", self.ema(self.spy.symbol, 60, Resolution.MINUTE))

        # Using the indexer to store our indicator as a custom property
        self.spy["bb"] = self.bb(self.spy.symbol, 20, 1, MovingAverageType.SIMPLE, Resolution.MINUTE)

        # Fee factor to be used by the custom fee model
        self.spy.fee_factor = 0.00002
        self.spy.set_fee_model(CustomFeeModel())

        # This property will be used to store the prices used to calculate the fees in order to assert the correct fee factor is used.
        self.spy.orders_fees_prices = {}

    def on_data(self, data):
        if not self.spy.fast_ema.is_ready:
            return

        if not self.portfolio.invested:
            # Using the property and the generic interface to access our indicator
            if self.spy.slow_ema > self.spy.fast_ema:
                self.set_holdings(self.spy.symbol, 1)
        else:
            if self.spy.get[ExponentialMovingAverage]("slow_ema") < self.spy.get[ExponentialMovingAverage]("fast_ema"):
                self.liquidate(self.spy.symbol)

        # Using the indexer to access our indicator
        bb: BollingerBands = self.spy["bb"]
        self.plot("bb", bb.upper_band, bb.middle_band, bb.lower_band)

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            fee = order_event.order_fee
            expected_fee = self.spy.orders_fees_prices[order_event.order_id] * order_event.absolute_fill_quantity * self.spy.fee_factor
            if not isclose(fee.value.amount, expected_fee, rel_tol=1e-15):
                raise Exception(f"Custom fee model failed to set the correct fee. Expected: {expected_fee}. Actual: {fee.value.amount}")

    def on_end_of_algorithm(self):
        if self.transactions.orders_count == 0:
            raise Exception("No orders executed")

class CustomFeeModel(FeeModel):
    '''This custom fee is implemented for demonstration purposes only.'''

    def get_order_fee(self, parameters):
        security = parameters.security
        # custom fee math using the fee factor stored in security instance
        fee_factor = security.fee_factor
        if fee_factor is None:
            fee_factor = 0.00001

        # Store the price used to calculate the fee for this order
        security["orders_fees_prices"][parameters.order.id] = security.price

        fee = max(1.0, security.price * parameters.order.absolute_quantity * fee_factor)

        return OrderFee(CashAmount(fee, "USD"))
