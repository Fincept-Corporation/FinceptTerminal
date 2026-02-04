# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5B90E12B
# Category: Benchmark
# Description: Coarse Fine Universe Selection Benchmark
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class CoarseFineUniverseSelectionBenchmark(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2017, 11, 1)
        self.set_end_date(2018, 3, 1)
        self.set_cash(50000)

        self.universe_settings.resolution = Resolution.MINUTE

        self.add_universe(self.coarse_selection_function, self.fine_selection_function)

        self.number_of_symbols = 150
        self.number_of_symbols_fine = 40
        self._changes = None

    # sort the data by daily dollar volume and take the top 'NumberOfSymbols'
    def coarse_selection_function(self, coarse):

        selected = [x for x in coarse if (x.has_fundamental_data)]
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted(selected, key=lambda x: x.dollar_volume, reverse=True)

        # return the symbol objects of the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_dollar_volume[:self.number_of_symbols] ]

    # sort the data by P/E ratio and take the top 'NumberOfSymbolsFine'
    def fine_selection_function(self, fine):
        # sort descending by P/E ratio
        sorted_by_pe_ratio = sorted(fine, key=lambda x: x.valuation_ratios.pe_ratio, reverse=True)
        # take the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_pe_ratio[:self.number_of_symbols_fine] ]

    def on_data(self, data):
        # if we have no changes, do nothing
        if self._changes is None: return

        # liquidate removed securities
        for security in self._changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        for security in self._changes.added_securities:
            self.set_holdings(security.symbol, 0.02)
        self._changes = None

    def on_securities_changed(self, changes):
        self._changes = changes
