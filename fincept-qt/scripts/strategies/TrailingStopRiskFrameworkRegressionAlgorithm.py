# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-DD7ECCB4
# Category: Risk Management
# Description: Show example of how to use the TrailingStopRiskManagementModel
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from BaseFrameworkRegressionAlgorithm import BaseFrameworkRegressionAlgorithm
from Risk.TrailingStopRiskManagementModel import TrailingStopRiskManagementModel

class TrailingStopRiskFrameworkRegressionAlgorithm(BaseFrameworkRegressionAlgorithm):
    '''Show example of how to use the TrailingStopRiskManagementModel'''

    def initialize(self):
        super().initialize()
        self.set_universe_selection(ManualUniverseSelectionModel([Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)]))

        self.set_risk_management(TrailingStopRiskManagementModel(0.01))
