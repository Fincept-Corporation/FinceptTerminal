# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-AE6D38CF
# Category: Universe Selection
# Description: Regression algorithm asserting the behavior of a ScheduledUniverse
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting the behavior of a ScheduledUniverse
### </summary>
class BasicTemplateAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10, 8)
        
        self._spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        self._selection_time =[ datetime(2013, 10, 7, 1, 0, 0), datetime(2013, 10, 8, 1, 0, 0)]

        self.add_universe(ScheduledUniverse(self.date_rules.every_day(), self.time_rules.at(1, 0), self.select_assets))


    def select_assets(self, time):
        self.debug(f"Universe selection called: {Time}")
        expected_time = self._selection_time.pop(0)
        if expected_time != self.time:
            raise ValueError(f"Unexpected selection time {self.time} expected {expected_time}")

        return [ self._spy ]
    
    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings(self._spy, 1)

    def on_end_of_algorithm(self):
        if len(self._selection_time) > 0:
            raise ValueError("Unexpected selection times")
