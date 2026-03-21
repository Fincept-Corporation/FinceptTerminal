# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-2D03F945
# Category: Execution Model
# Description: Regression algorithm for the VolumeWeightedAveragePriceExecutionModel. This algorithm shows how the execution model w...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Alphas.RsiAlphaModel import RsiAlphaModel
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel
from Execution.VolumeWeightedAveragePriceExecutionModel import VolumeWeightedAveragePriceExecutionModel

### <summary>
### Regression algorithm for the VolumeWeightedAveragePriceExecutionModel.
### This algorithm shows how the execution model works to split up orders and
### submit them only when the price is on the favorable side of the intraday VWAP.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class VolumeWeightedAveragePriceExecutionModelRegressionAlgorithm(QCAlgorithm):
    '''Regression algorithm for the VolumeWeightedAveragePriceExecutionModel.
    This algorithm shows how the execution model works to split up orders and
    submit them only when the price is on the favorable side of the intraday VWAP.'''

    def initialize(self):

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

        # using hourly rsi to generate more insights
        self.set_alpha(RsiAlphaModel(14, Resolution.HOUR))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(VolumeWeightedAveragePriceExecutionModel())

        self.insights_generated += self.on_insights_generated

    def on_insights_generated(self, algorithm, data):
        self.log(f"{self.time}: {', '.join(str(x) for x in data.insights)}")

    def on_order_event(self, orderEvent):
        self.log(f"{self.time}: {orderEvent}")
