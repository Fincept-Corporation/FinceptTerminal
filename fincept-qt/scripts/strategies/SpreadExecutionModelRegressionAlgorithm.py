# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E4FEA7D1
# Category: Execution Model
# Description: Regression algorithm for the SpreadExecutionModel. This algorithm shows how the execution model works to submit order...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Alphas.RsiAlphaModel import RsiAlphaModel
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel
from Execution.SpreadExecutionModel import SpreadExecutionModel

### <summary>
### Regression algorithm for the SpreadExecutionModel.
### This algorithm shows how the execution model works to
### submit orders only when the price is on desirably tight spread.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class SpreadExecutionModelRegressionAlgorithm(QCAlgorithm):
    '''Regression algorithm for the SpreadExecutionModel.
    This algorithm shows how the execution model works to
    submit orders only when the price is on desirably tight spread.'''

    def initialize(self):
        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)

        self.set_universe_selection(ManualUniverseSelectionModel([
            Symbol.create('AIG', SecurityType.EQUITY, Market.USA),
            Symbol.create('BAC', SecurityType.EQUITY, Market.USA),
            Symbol.create('IBM', SecurityType.EQUITY, Market.USA),
            Symbol.create('SPY', SecurityType.EQUITY, Market.USA)
        ]))

        # using hourly rsi to generate more insights
        self.set_alpha(RsiAlphaModel(14, Resolution.HOUR))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(SpreadExecutionModel())

        self.insights_generated += self.on_insights_generated

    def on_insights_generated(self, algorithm, data):
        self.log(f"{self.time}: {', '.join(str(x) for x in data.insights)}")

    def on_order_event(self, order_event):
        self.log(f"{self.time}: {order_event}")
