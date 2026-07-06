# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9874FF18
# Category: Risk Management
# Description: Test algorithm using 'QCAlgorithm.add_risk_management(IRiskManagementModel)'
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Test algorithm using 'QCAlgorithm.add_risk_management(IRiskManagementModel)'
### </summary>
class AddRiskManagementAlgorithm(QCAlgorithm):
    '''Basic template framework algorithm uses framework components to define the algorithm.'''

    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        symbols = [ Symbol.create("SPY", SecurityType.EQUITY, Market.USA) ]

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

        # Both setting methods should work
        risk_model = CompositeRiskManagementModel(MaximumDrawdownPercentPortfolio(0.02))
        risk_model.add_risk_management(MaximumUnrealizedProfitPercentPerSecurity(0.01))

        self.set_risk_management(MaximumDrawdownPercentPortfolio(0.02))
        self.add_risk_management(MaximumUnrealizedProfitPercentPerSecurity(0.01))

