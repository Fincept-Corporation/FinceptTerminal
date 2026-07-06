# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-50C4F04D
# Category: Alpha Model
# Description: Show cases how to use the CompositeAlphaModel to define
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from Alphas.RsiAlphaModel import RsiAlphaModel
from Alphas.EmaCrossAlphaModel import EmaCrossAlphaModel
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel

### <summary>
### Show cases how to use the CompositeAlphaModel to define.
### </summary>
class CompositeAlphaModelFrameworkAlgorithm(QCAlgorithm):
    '''Show cases how to use the CompositeAlphaModel to define.'''

    def initialize(self):
        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        # even though we're using a framework algorithm, we can still add our securities
        # using the AddEquity/Forex/Crypto/ect methods and then pass them into a manual
        # universe selection model using securities.keys()
        self.add_equity("SPY")
        self.add_equity("IBM")
        self.add_equity("BAC")
        self.add_equity("AIG")

        # define a manual universe of all the securities we manually registered
        self.set_universe_selection(ManualUniverseSelectionModel())

        # define alpha model as a composite of the rsi and ema cross models
        self.set_alpha(CompositeAlphaModel(RsiAlphaModel(), EmaCrossAlphaModel()))

        # default models for the rest
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())
