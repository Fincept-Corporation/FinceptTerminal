# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9EECE18B
# Category: General Strategy
# Description: Test algorithm using 'ConfidenceWeightedPortfolioConstructionModel' and 'ConstantAlphaModel' generating a constant 'I...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Test algorithm using 'ConfidenceWeightedPortfolioConstructionModel' and 'ConstantAlphaModel'
### generating a constant 'Insight' with a 0.25 confidence
### </summary>
class ConfidenceWeightedFrameworkAlgorithm(QCAlgorithm):
    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        symbols = [ Symbol.create("SPY", SecurityType.EQUITY, Market.USA) ]

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, 0.25))
        self.set_portfolio_construction(ConfidenceWeightedPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

    def on_end_of_algorithm(self):
        # holdings value should be 0.25 - to avoid price fluctuation issue we compare with 0.28 and 0.23
        if (self.portfolio.total_holdings_value > self.portfolio.total_portfolio_value * 0.28
            or self.portfolio.total_holdings_value < self.portfolio.total_portfolio_value * 0.23):
            raise ValueError("Unexpected Total Holdings Value: " + str(self.portfolio.total_holdings_value))
