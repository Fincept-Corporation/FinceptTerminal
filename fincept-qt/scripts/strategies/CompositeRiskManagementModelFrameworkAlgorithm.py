# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5A1D14B7
# Category: Risk Management
# Description: Show cases how to use the CompositeRiskManagementModel
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Show cases how to use the CompositeRiskManagementModel.
### </summary>
class CompositeRiskManagementModelFrameworkAlgorithm(QCAlgorithm):
    '''Show cases how to use the CompositeRiskManagementModel.'''

    def initialize(self):

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel([Symbol.create("SPY", SecurityType.EQUITY, Market.USA)]))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

        # define risk management model as a composite of several risk management models
        self.set_risk_management(CompositeRiskManagementModel(
            MaximumUnrealizedProfitPercentPerSecurity(0.01),
            MaximumDrawdownPercentPerSecurity(0.01)
        ))
