# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9159A849
# Category: Risk Management
# Description: This example algorithm defines its own custom coarse/fine fundamental selection model with equally weighted portfolio...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel
from Alphas.ConstantAlphaModel import ConstantAlphaModel
from Execution.ImmediateExecutionModel import ImmediateExecutionModel
from Risk.MaximumSectorExposureRiskManagementModel import MaximumSectorExposureRiskManagementModel

### <summary>
### This example algorithm defines its own custom coarse/fine fundamental selection model
### with equally weighted portfolio and a maximum sector exposure.
### </summary>
class SectorExposureRiskFrameworkAlgorithm(QCAlgorithm):
    '''This example algorithm defines its own custom coarse/fine fundamental selection model
### with equally weighted portfolio and a maximum sector exposure.'''

    def initialize(self):

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.DAILY

        self.set_start_date(2014, 3, 25)
        self.set_end_date(2014, 4, 7)
        self.set_cash(100000)

        # set algorithm framework models
        self.set_universe_selection(FineFundamentalUniverseSelectionModel(self.select_coarse, self.select_fine))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1)))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_risk_management(MaximumSectorExposureRiskManagementModel())

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug(f"Order event: {order_event}. Holding value: {self.securities[order_event.symbol].holdings.absolute_holdings_value}")

    def select_coarse(self, coarse):
        tickers = ["AAPL", "AIG", "IBM"] if self.time.date() < date(2014, 4, 1) else [ "GOOG", "BAC", "SPY" ]
        return [Symbol.create(x, SecurityType.EQUITY, Market.USA) for x in tickers]

    def select_fine(self, fine):
        return [f.symbol for f in fine]
