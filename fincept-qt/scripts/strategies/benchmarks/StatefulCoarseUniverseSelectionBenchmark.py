# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-FA9F3E6A
# Category: Benchmark
# Description: Stateful Coarse Universe Selection Benchmark
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class StatefulCoarseUniverseSelectionBenchmark(QCAlgorithm):

    def initialize(self):
        self.universe_settings.resolution = Resolution.DAILY

        self.set_start_date(2017, 1, 1)
        self.set_end_date(2019, 1, 1)
        self.set_cash(50000)

        self.add_universe(self.coarse_selection_function)
        self.number_of_symbols = 250
        self._black_list = []

    # sort the data by daily dollar volume and take the top 'NumberOfSymbols'
    def coarse_selection_function(self, coarse):

        selected = [x for x in coarse if (x.has_fundamental_data)]
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted(selected, key=lambda x: x.dollar_volume, reverse=True)

        # return the symbol objects of the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_dollar_volume[:self.number_of_symbols] if not (x.symbol in self._black_list) ]

    def on_data(self, slice):
        if slice.has_data:
            symbol = slice.keys()[0]
            if symbol:
                if len(self._black_list) > 50:
                    self._black_list.pop(0)
                self._black_list.append(symbol)

    def on_securities_changed(self, changes):
        # if we have no changes, do nothing
        if changes is None: return

        # liquidate removed securities
        for security in changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        for security in changes.added_securities:
            self.set_holdings(security.symbol, 0.001)
