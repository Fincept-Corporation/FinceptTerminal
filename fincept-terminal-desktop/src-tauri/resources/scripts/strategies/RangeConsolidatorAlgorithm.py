# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-052CED9D
# Category: Data Consolidation
# Description: Example algorithm of how to use RangeConsolidator
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example algorithm of how to use RangeConsolidator
### </summary>
class RangeConsolidatorAlgorithm(QCAlgorithm):
    def get_resolution(self):
        return Resolution.DAILY

    def get_range(self):
        return 100

    def initialize(self):        
        self.set_start_and_end_dates();
        self.add_equity("SPY", self.get_resolution())
        range_consolidator = self.create_range_consolidator()
        range_consolidator.data_consolidated += self.on_data_consolidated
        self.first_data_consolidated = None;

        self.subscription_manager.add_consolidator("SPY", range_consolidator)

    def set_start_and_end_dates(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

    def on_end_of_algorithm(self):
        if self.first_data_consolidated == None:
            raise Exception("The consolidator should have consolidated at least one RangeBar, but it did not consolidated any one")

    def create_range_consolidator(self):
        return RangeConsolidator(self.get_range())

    def on_data_consolidated(self, sender, range_bar):
        if (self.first_data_consolidated is None):
            self.first_data_consolidated = range_bar

        if round(range_bar.high - range_bar.low, 2) != self.get_range() * 0.01: # The minimum price change for SPY is 0.01, therefore the range size of each bar equals Range * 0.01
            raise Exception(f"The difference between the High and Low for all RangeBar's should be {self.get_range() * 0.01}, but for this RangeBar was {round(range_bar.low - range_bar.high, 2)}")
