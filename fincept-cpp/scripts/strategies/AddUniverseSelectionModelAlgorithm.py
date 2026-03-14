# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-8AB253F8
# Category: Universe Selection
# Description: Test algorithm using 'QCAlgorithm.add_universe_selection(IUniverseSelectionModel)'
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Test algorithm using 'QCAlgorithm.add_universe_selection(IUniverseSelectionModel)'
### </summary>
class AddUniverseSelectionModelAlgorithm(QCAlgorithm):
    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,8)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self.universe_settings.resolution = Resolution.DAILY

        # set algorithm framework models
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

        self.set_universe_selection(ManualUniverseSelectionModel([ Symbol.create("SPY", SecurityType.EQUITY, Market.USA) ]))
        self.add_universe_selection(ManualUniverseSelectionModel([ Symbol.create("AAPL", SecurityType.EQUITY, Market.USA) ]))
        self.add_universe_selection(ManualUniverseSelectionModel(
                Symbol.create("SPY", SecurityType.EQUITY, Market.USA), # duplicate will be ignored
                Symbol.create("FB", SecurityType.EQUITY, Market.USA)))

    def on_end_of_algorithm(self):
        if self.universe_manager.count != 3:
            raise ValueError("Unexpected universe count")
        if self.universe_manager.active_securities.count != 3:
            raise ValueError("Unexpected active securities")
