# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-98E14661
# Category: Options
# Description: This regression algorithm checks if all the option chain data coming to the algo is consistent with current securitie...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm checks if all the option chain data coming to the algo is consistent with current securities manager state
### </summary>
### <meta name="tag" content="regression test" />
### <meta name="tag" content="options" />
### <meta name="tag" content="using data" />
### <meta name="tag" content="filter selection" />
class OptionChainConsistencyRegressionAlgorithm(QCAlgorithm):

    underlying_ticker = "GOOG"

    def initialize(self):

        self.set_cash(10000)
        self.set_start_date(2015,12,24)
        self.set_end_date(2015,12,24)

        self.equity = self.add_equity(self.underlying_ticker);
        self.option = self.add_option(self.underlying_ticker);

        # set our strike/expiry filter for this option chain
        self.option.set_filter(self.universe_func)

        self.set_benchmark(self.equity.symbol)

    def on_data(self, slice):
        if self.portfolio.invested: return
        for kvp in slice.option_chains:
            chain = kvp.value
            for o in chain:
                if not self.securities.contains_key(o.symbol):
                    self.log("Inconsistency found: option chains contains contract {0} that is not available in securities manager and not available for trading".format(o.symbol.value))           

            contracts = filter(lambda x: x.expiry.date() == self.time.date() and
                                         x.strike < chain.underlying.price and
                                         x.right == OptionRight.CALL, chain)

            sorted_contracts = sorted(contracts, key = lambda x: x.strike, reverse = True)

            if len(sorted_contracts) > 2:
                self.market_order(sorted_contracts[2].symbol, 1)
                self.market_on_close_order(sorted_contracts[2].symbol, -1)

    # set our strike/expiry filter for this option chain
    def universe_func(self, universe):
        return universe.include_weeklys().strikes(-2, 2).expiration(timedelta(0), timedelta(10))

    def on_order_event(self, order_event):
        self.log(str(order_event))
