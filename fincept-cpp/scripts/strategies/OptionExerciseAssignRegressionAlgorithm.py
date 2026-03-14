# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A9A9EB72
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
class OptionExerciseAssignRegressionAlgorithm(QCAlgorithm):

    underlying_ticker = "GOOG"

    def initialize(self):
        self.set_cash(100000)
        self.set_start_date(2015,12,24)
        self.set_end_date(2015,12,28)

        self.equity = self.add_equity(self.underlying_ticker)
        self.option = self.add_option(self.underlying_ticker)

        # set our strike/expiry filter for this option chain
        self.option.set_filter(self.universe_func)

        self.set_benchmark(self.equity.symbol)
        self._assigned_option = False

    def on_data(self, slice):
        if self.portfolio.invested: return
        for kvp in slice.option_chains:
            chain = kvp.value
            # find the call options expiring today
            contracts = filter(lambda x:
                               x.expiry.date() == self.time.date() and
                               x.strike < chain.underlying.price and
                               x.right == OptionRight.CALL, chain)
            
            # sorted the contracts by their strikes, find the second strike under market price 
            sorted_contracts = sorted(contracts, key = lambda x: x.strike, reverse = True)[:2]

            if sorted_contracts:
                self.market_order(sorted_contracts[0].symbol, 1)
                self.market_order(sorted_contracts[1].symbol, -1)

    # set our strike/expiry filter for this option chain
    def universe_func(self, universe):
        return universe.include_weeklys().strikes(-2, 2).expiration(timedelta(0), timedelta(10))

    def on_order_event(self, order_event):
        self.log(str(order_event))

    def on_assignment_order_event(self, assignment_event):
        self.log(str(assignment_event))
        self._assigned_option = True
