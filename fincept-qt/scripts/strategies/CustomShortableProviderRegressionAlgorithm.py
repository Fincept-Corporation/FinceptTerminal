# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-324DD834
# Category: Regression Test
# Description: Regression algorithm asserting we can specify a custom Shortable Provider
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting we can specify a custom Shortable Provider
### </summary>
class CustomShortableProviderRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_cash(10000000)
        self.set_start_date(2013,10,4)
        self.set_end_date(2013,10,6)
        self.spy = self.add_security(SecurityType.EQUITY, "SPY", Resolution.DAILY)
        self.spy.set_shortable_provider(CustomShortableProvider())

    def on_data(self, data):
        spy_shortable_quantity = self.spy.shortable_provider.shortable_quantity(self.spy.symbol, self.time)
        if spy_shortable_quantity > 1000:
            self.order_id = self.sell("SPY", int(spy_shortable_quantity))

    def on_end_of_algorithm(self):
        transactions = self.transactions.orders_count
        if transactions != 1:
            raise Exception("Algorithm should have just 1 order, but was " + str(transactions))

        order_quantity = self.transactions.get_order_by_id(self.order_id).quantity
        if order_quantity != -1001:
            raise Exception("Quantity of order " + str(_order_id) + " should be " + str(-1001)+", but was {order_quantity}")
        
        fee_rate = self.spy.shortable_provider.fee_rate(self.spy.symbol, self.time)
        if fee_rate != 0.0025:
            raise Exception(f"Fee rate should be 0.0025, but was {fee_rate}")
        rebate_rate = self.spy.shortable_provider.rebate_rate(self.spy.symbol, self.time)
        if rebate_rate != 0.0507:
            raise Exception(f"Rebate rate should be 0.0507, but was {rebate_rate}")

class CustomShortableProvider(NullShortableProvider):
    def fee_rate(self, symbol: Symbol, local_time: DateTime):
        return 0.0025
    def rebate_rate(self, symbol: Symbol, local_time: DateTime):
        return 0.0507
    def shortable_quantity(self, symbol: Symbol, local_time: DateTime):
        if local_time < datetime(2013,10,4,16,0,0):
            return 10
        else:
            return 1001
