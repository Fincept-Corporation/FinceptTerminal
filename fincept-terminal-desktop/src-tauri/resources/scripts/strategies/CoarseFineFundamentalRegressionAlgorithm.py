# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-BF06E7EB
# Category: Regression Test
# Description: Demonstration of how to define a universe as a combination of use the coarse fundamental data and fine fundamental data
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of how to define a universe as a combination of use the coarse fundamental data and fine fundamental data
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
### <meta name="tag" content="regression test" />
class CoarseFineFundamentalRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014,3,24)   #Set Start Date
        self.set_end_date(2014,4,7)      #Set End Date
        self.set_cash(50000)            #Set Strategy Cash

        self.universe_settings.resolution = Resolution.DAILY

        # this add universe method accepts two parameters:
        # - coarse selection function: accepts an List[CoarseFundamental] and returns an List[Symbol]
        # - fine selection function: accepts an List[FineFundamental] and returns an List[Symbol]
        self.add_universe(self.coarse_selection_function, self.fine_selection_function)

        self.changes = None
        self.number_of_symbols_fine = 2

    # return a list of three fixed symbol objects
    def coarse_selection_function(self, coarse):
        tickers = [ "GOOG", "BAC", "SPY" ]

        if self.time.date() < date(2014, 4, 1):
            tickers = [ "AAPL", "AIG", "IBM" ]

        return [ Symbol.create(x, SecurityType.EQUITY, Market.USA) for x in tickers ]


    # sort the data by market capitalization and take the top 'number_of_symbols_fine'
    def fine_selection_function(self, fine):
        # sort descending by market capitalization
        sorted_by_market_cap = sorted(fine, key=lambda x: x.market_cap, reverse=True)

        # take the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_market_cap[:self.number_of_symbols_fine] ]

    def on_data(self, data):
        # if we have no changes, do nothing
        if self.changes is None: return

        # liquidate removed securities
        for security in self.changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)
                self.debug("Liquidated Stock: " + str(security.symbol.value))

        # we want 50% allocation in each security in our universe
        for security in self.changes.added_securities:
            if (security.fundamentals.earning_ratios.equity_per_share_growth.one_year > 0.25):
                self.set_holdings(security.symbol, 0.5)
                self.debug("Purchased Stock: " + str(security.symbol.value))

        self.changes = None

    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        self.changes = changes
