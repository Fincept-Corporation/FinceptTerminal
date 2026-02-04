# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0A8C8781
# Category: Universe Selection
# Description: Demonstration of how to define a universe using the fundamental data
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of how to define a universe using the fundamental data
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
### <meta name="tag" content="regression test" />
class FundamentalUniverseSelectionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 3, 25)
        self.set_end_date(2014, 4, 7)

        self.universe_settings.resolution = Resolution.DAILY

        self.add_equity("SPY")
        self.add_equity("AAPL")

        self.set_universe_selection(FundamentalUniverseSelectionModel(self.select))

        self.changes = None

    # return a list of three fixed symbol objects
    def selection_function(self, fundamental):
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted([x for x in fundamental if x.price > 1],
            key=lambda x: x.dollar_volume, reverse=True)

        # sort descending by P/E ratio
        sorted_by_pe_ratio = sorted(sorted_by_dollar_volume, key=lambda x: x.valuation_ratios.pe_ratio, reverse=True)

        # take the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_pe_ratio[:self.number_of_symbols_fundamental] ]

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
            self.set_holdings(security.symbol, 0.02)

        self.changes = None

    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        self.changes = changes

    def select(self, fundamental):
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted([x for x in fundamental if x.has_fundamental_data and x.price > 1],
            key=lambda x: x.dollar_volume, reverse=True)

        # sort descending by P/E ratio
        sorted_by_pe_ratio = sorted(sorted_by_dollar_volume, key=lambda x: x.valuation_ratios.pe_ratio, reverse=True)

        # take the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_pe_ratio[:2] ]
