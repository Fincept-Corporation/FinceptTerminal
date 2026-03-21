# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-AE32C215
# Category: Universe Selection
# Description: Framework algorithm that uses the EmaCrossUniverseSelectionModel to select the universe based on a moving average cross
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Alphas.ConstantAlphaModel import ConstantAlphaModel
from Selection.EmaCrossUniverseSelectionModel import EmaCrossUniverseSelectionModel
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel

### <summary>
### Framework algorithm that uses the EmaCrossUniverseSelectionModel to
### select the universe based on a moving average cross.
### </summary>
class EmaCrossUniverseSelectionFrameworkAlgorithm(QCAlgorithm):
    '''Framework algorithm that uses the EmaCrossUniverseSelectionModel to select the universe based on a moving average cross.'''

    def initialize(self):

        self.set_start_date(2013,1,1)
        self.set_end_date(2015,1,1)
        self.set_cash(100000)

        fast_period = 100
        slow_period = 300
        count = 10

        self.universe_settings.leverage = 2.0
        self.universe_settings.resolution = Resolution.DAILY

        self.set_universe_selection(EmaCrossUniverseSelectionModel(fast_period, slow_period, count))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1), None, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
