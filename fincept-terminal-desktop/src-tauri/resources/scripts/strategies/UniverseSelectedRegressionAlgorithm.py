# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-BAF35694
# Category: Universe Selection
# Description: Regression algorithm asserting the behavior of Universe.SELECTED collection
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting the behavior of Universe.SELECTED collection
### </summary>
class UniverseSelectedRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 3, 25)
        self.set_end_date(2014, 3, 27)

        self.universe_settings.resolution = Resolution.DAILY

        self._universe = self.add_universe(self.selection_function)
        self.selection_count = 0

    def selection_function(self, fundamentals):
        sorted_by_dollar_volume = sorted(fundamentals, key=lambda x: x.dollar_volume, reverse=True)

        sorted_by_dollar_volume = sorted_by_dollar_volume[self.selection_count:]
        self.selection_count = self.selection_count + 1

        # return the symbol objects of the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_dollar_volume[:self.selection_count] ]

    def on_data(self, data):
        if Symbol.create("TSLA", SecurityType.EQUITY, Market.USA) in self._universe.selected:
            raise ValueError(f"TSLA shouldn't of been selected")

        self.buy(next(iter(self._universe.selected)), 1)

    def on_end_of_algorithm(self):
        if self.selection_count != 3:
            raise ValueError(f"Unexpected selection count {self.selection_count}")
        if self._universe.selected.count != 3 or self._universe.selected.count == self._universe.members.count:
            raise ValueError(f"Unexpected universe selected count {self._universe.selected.count}")
