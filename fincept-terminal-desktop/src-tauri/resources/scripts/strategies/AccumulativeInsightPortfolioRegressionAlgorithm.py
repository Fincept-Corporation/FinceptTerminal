# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-19E8A564
# Category: Portfolio Management
# Description: Test algorithm using 'AccumulativeInsightPortfolioConstructionModel.py' and 'ConstantAlphaModel' generating a constan...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Test algorithm using 'AccumulativeInsightPortfolioConstructionModel.py' and 'ConstantAlphaModel'
### generating a constant 'Insight'
### </summary>
class AccumulativeInsightPortfolioRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        symbols = [ Symbol.create("SPY", SecurityType.EQUITY, Market.USA) ]

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, 0.25))
        self.set_portfolio_construction(AccumulativeInsightPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

    def on_end_of_algorithm(self):
        # holdings value should be 0.03 - to avoid price fluctuation issue we compare with 0.06 and 0.01
        if (self.portfolio.total_holdings_value > self.portfolio.total_portfolio_value * 0.06
            or self.portfolio.total_holdings_value < self.portfolio.total_portfolio_value * 0.01):
            raise ValueError("Unexpected Total Holdings Value: " + str(self.portfolio.total_holdings_value))
