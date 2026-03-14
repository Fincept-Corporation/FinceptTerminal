# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C395D1D2
# Category: Regression Test
# Description: Example algorithm using and asserting the behavior of auxiliary data handlers
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example algorithm using and asserting the behavior of auxiliary data handlers
### </summary>
class AuxiliaryDataHandlersRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2007, 5, 16)
        self.set_end_date(2015, 1, 1)

        self.universe_settings.resolution = Resolution.DAILY

        # will get delisted
        self.add_equity("AAA.1")

        # get's remapped
        self.add_equity("SPWR")

        # has a split & dividends
        self.add_equity("AAPL")

    def on_delistings(self, delistings: Delistings):
        self._on_delistings_called = True

    def on_symbol_changed_events(self, symbolsChanged: SymbolChangedEvents):
        self._on_symbol_changed_events = True

    def on_splits(self, splits: Splits):
        self._on_splits = True

    def on_dividends(self, dividends: Dividends):
        self._on_dividends = True

    def on_end_of_algorithm(self):
        if not self._on_delistings_called:
            raise ValueError("OnDelistings was not called!")
        if not self._on_symbol_changed_events:
            raise ValueError("OnSymbolChangedEvents was not called!")
        if not self._on_splits:
            raise ValueError("OnSplits was not called!")
        if not self._on_dividends:
            raise ValueError("OnDividends was not called!")
