# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-CB434E24
# Category: Universe Selection
# Description: Regression algorithm showing how to implement a custom universe selection model and asserting it's behavior
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Selection.FundamentalUniverseSelectionModel import FundamentalUniverseSelectionModel

### <summary>
### Regression algorithm showing how to implement a custom universe selection model and asserting it's behavior
### </summary>
class CustomUniverseSelectionModelRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014,3,24)
        self.set_end_date(2014,4,7)

        self.universe_settings.resolution = Resolution.DAILY
        self.set_universe_selection(CustomUniverseSelectionModel())

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            for kvp in self.active_securities:
                self.set_holdings(kvp.key, 0.1)

class CustomUniverseSelectionModel(FundamentalUniverseSelectionModel):

    def __init__(self, universe_settings = None):
        super().__init__(universe_settings)
        self._selected = False

    def select(self, algorithm, fundamental):
        if not self._selected:
            self._selected = True
            return [ Symbol.create('AAPL', SecurityType.EQUITY, Market.USA) ]
        return Universe.UNCHANGED
