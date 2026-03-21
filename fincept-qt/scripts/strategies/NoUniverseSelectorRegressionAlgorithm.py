# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-994B4750
# Category: Universe Selection
# Description: Custom data universe selection regression algorithm asserting it's behavior. See GH issue #6396
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Custom data universe selection regression algorithm asserting it's behavior. See GH issue #6396
### </summary>
class NoUniverseSelectorRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2014, 3, 24)
        self.set_end_date(2014, 3, 31)

        self.universe_settings.resolution = Resolution.DAILY;
        self.add_universe(CoarseFundamental)
        self.changes = None

    def on_data(self, data):
        # if we have no changes, do nothing
        if not self.changes: return

        # liquidate removed securities
        for security in self.changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        active_and_with_data_securities = sum(x.value.has_data for x in self.active_securities)
        # we want 1/N allocation in each security in our universe
        for security in self.changes.added_securities:
            if security.has_data:
                self.set_holdings(security.symbol, 1 / active_and_with_data_securities)
        self.changes = None

    def on_securities_changed(self, changes):
        self.changes = changes
