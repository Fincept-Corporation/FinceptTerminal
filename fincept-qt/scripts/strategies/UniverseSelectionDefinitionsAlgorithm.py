# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-DBD4926F
# Category: Universe Selection
# Description: This algorithm shows some of the various helper methods available when defining universes
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This algorithm shows some of the various helper methods available when defining universes
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
class UniverseSelectionDefinitionsAlgorithm(QCAlgorithm):

    def initialize(self):
        # subscriptions added via universe selection will have this resolution
        self.universe_settings.resolution = Resolution.DAILY

        self.set_start_date(2014,3,24)    # Set Start Date
        self.set_end_date(2014,3,28)     # Set End Date
        self.set_cash(100000)            # Set Strategy Cash

        # add universe for the top 3 stocks by dollar volume
        self.add_universe(self.universe.top(3))

        self.changes = None
        self.on_securities_changed_was_called = False

    def on_data(self, data):
        if self.changes is None: return

        # liquidate securities that fell out of our universe
        for security in self.changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        # invest in securities just added to our universe
        for security in self.changes.added_securities:
            if not security.invested:
                self.market_order(security.symbol, 10)

        self.changes = None

    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        self.changes = changes
        self.on_securities_changed_was_called = True

    def on_end_of_algorithm(self):
        if not self.on_securities_changed_was_called:
            raise Exception("OnSecuritiesChanged() method was never called!")
