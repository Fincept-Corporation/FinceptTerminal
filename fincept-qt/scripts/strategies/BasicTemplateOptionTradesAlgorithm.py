# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-29268ED2
# Category: Options
# Description: This example demonstrates how to add options for a given underlying equity security. It also shows how you can prefil...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This example demonstrates how to add options for a given underlying equity security.
### It also shows how you can prefilter contracts easily based on strikes and expirations.
### It also shows how you can inspect the option chain to pick a specific option contract to trade.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="options" />
### <meta name="tag" content="filter selection" />
class BasicTemplateOptionTradesAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(100000)

        option = self.add_option("GOOG")

        # add the initial contract filter 
        # SetFilter method accepts timedelta objects or integer for days.
        # The following statements yield the same filtering criteria
        option.set_filter(-2, +2, 0, 10)
        # option.set_filter(-2, +2, timedelta(0), timedelta(10))

        # use the underlying equity as the benchmark
        self.set_benchmark("GOOG")

    def on_data(self,slice):
        if not self.portfolio.invested:
            for kvp in slice.option_chains:
                chain = kvp.value
                # find the second call strike under market price expiring today
                contracts = sorted(sorted(chain, key = lambda x: abs(chain.underlying.price - x.strike)),
                                                 key = lambda x: x.expiry, reverse=False)

                if len(contracts) == 0: continue
                if contracts[0] != None:
                    self.market_order(contracts[0].symbol, 1)
        else:
            self.liquidate()

        for kpv in slice.bars:
            self.log("---> OnData: {0}, {1}, {2}".format(self.time, kpv.key.value, str(kpv.value.close)))

    def on_order_event(self, order_event):
        self.log(str(order_event))
