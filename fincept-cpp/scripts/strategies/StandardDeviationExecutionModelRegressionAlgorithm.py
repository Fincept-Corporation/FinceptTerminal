# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C7F868BA
# Category: Execution Model
# Description: Regression algorithm for the StandardDeviationExecutionModel. This algorithm shows how the execution model works to s...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Alphas.RsiAlphaModel import RsiAlphaModel
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel
from Execution.StandardDeviationExecutionModel import StandardDeviationExecutionModel

### <summary>
### Regression algorithm for the StandardDeviationExecutionModel.
### This algorithm shows how the execution model works to split up orders and submit them
### only when the price is 2 standard deviations from the 60min mean (default model settings).
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class StandardDeviationExecutionModelRegressionAlgorithm(QCAlgorithm):
    '''Regression algorithm for the StandardDeviationExecutionModel.
    This algorithm shows how the execution model works to split up orders and submit them
    only when the price is 2 standard deviations from the 60min mean (default model settings).'''

    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)
        self.set_cash(1000000)

        self.set_universe_selection(ManualUniverseSelectionModel([
            Symbol.create('AIG', SecurityType.EQUITY, Market.USA),
            Symbol.create('BAC', SecurityType.EQUITY, Market.USA),
            Symbol.create('IBM', SecurityType.EQUITY, Market.USA),
            Symbol.create('SPY', SecurityType.EQUITY, Market.USA)
        ]))

        self.set_alpha(RsiAlphaModel(14, Resolution.HOUR))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(StandardDeviationExecutionModel())

    def on_order_event(self, order_event):
        self.log(f"{self.time}: {order_event}")
