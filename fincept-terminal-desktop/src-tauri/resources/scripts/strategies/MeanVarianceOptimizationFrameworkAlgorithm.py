# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C42B3790
# Category: General Strategy
# Description: Mean Variance Optimization algorithm Uses the HistoricalReturnsAlphaModel and the MeanVarianceOptimizationPortfolioCo...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Portfolio.MeanVarianceOptimizationPortfolioConstructionModel import *

### <summary>
### Mean Variance Optimization algorithm
### Uses the HistoricalReturnsAlphaModel and the MeanVarianceOptimizationPortfolioConstructionModel
### to create an algorithm that rebalances the portfolio according to modern portfolio theory
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class MeanVarianceOptimizationFrameworkAlgorithm(QCAlgorithm):
    '''Mean Variance Optimization algorithm.'''

    def initialize(self):

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        self.settings.rebalance_portfolio_on_insight_changes = False

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self._symbols = [ Symbol.create(x, SecurityType.EQUITY, Market.USA) for x in [ 'AIG', 'BAC', 'IBM', 'SPY' ] ]

        # set algorithm framework models
        self.set_universe_selection(CoarseFundamentalUniverseSelectionModel(self.coarse_selector))
        self.set_alpha(HistoricalReturnsAlphaModel(resolution = Resolution.DAILY))
        self.set_portfolio_construction(MeanVarianceOptimizationPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())

    def coarse_selector(self, coarse):
        # Drops SPY after the 8th
        last = 3 if self.time.day > 8 else len(self._symbols)

        return self._symbols[0:last]

    def on_order_event(self,  order_event):
        if order_event.status == OrderStatus.FILLED:
            self.log(str(order_event))
