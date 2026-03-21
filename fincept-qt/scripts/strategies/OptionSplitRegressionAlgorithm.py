# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D07A1F27
# Category: Options
# Description: This regression algorithm tests option exercise and assignment functionality We open two positions and go with them i...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests option exercise and assignment functionality
### We open two positions and go with them into expiration. We expect to see our long position exercised and short position assigned.
### </summary>
### <meta name="tag" content="regression test" />
### <meta name="tag" content="options" />
class OptionSplitRegressionAlgorithm(QCAlgorithm):

    def initialize(self):

        # this test opens position in the first day of trading, lives through stock split (7 for 1),
        # and closes adjusted position on the second day

        self.set_cash(1000000)
        self.set_start_date(2014,6,6)
        self.set_end_date(2014,6,9)

        option = self.add_option("AAPL")

        # set our strike/expiry filter for this option chain
        option.set_filter(self.universe_func)

        self.set_benchmark("AAPL")
        self.contract = None

    def on_data(self, slice):
        if not self.portfolio.invested:
            if self.time.hour > 9 and self.time.minute > 0:
                for kvp in slice.option_chains:
                    chain = kvp.value
                    contracts = filter(lambda x: x.strike == 650 and x.right ==  OptionRight.CALL, chain)
                    sorted_contracts = sorted(contracts, key = lambda x: x.expiry)

                if len(sorted_contracts) > 1:
                    self.contract = sorted_contracts[1]
                    self.buy(self.contract.symbol, 1)

        elif self.time.day > 6 and self.time.hour > 14 and self.time.minute > 0:
            self.liquidate()

        if self.portfolio.invested:
            options_hold = [x for x in self.portfolio.securities if x.value.holdings.absolute_quantity != 0]
            holdings = options_hold[0].value.holdings.absolute_quantity
            if self.time.day == 6 and holdings != 1:
                self.log("Expected position quantity of 1 but was {0}".format(holdings))
            if self.time.day == 9 and holdings != 7:
                self.log("Expected position quantity of 7 but was {0}".format(holdings))

    # set our strike/expiry filter for this option chain
    def universe_func(self, universe):
        return universe.include_weeklys().strikes(-2, 2).expiration(timedelta(0), timedelta(365*2))

    def on_order_event(self, order_event):
        self.log(str(order_event))
