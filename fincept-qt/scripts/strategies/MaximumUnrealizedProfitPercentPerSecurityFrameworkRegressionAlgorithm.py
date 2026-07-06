# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-7E35ADAA
# Category: Regression Test
# Description: Regression algorithm to assert the behavior of <see cref="MaximumUnrealizedProfitPercentPerSecurity"/>
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from BaseFrameworkRegressionAlgorithm import BaseFrameworkRegressionAlgorithm
from Risk.MaximumUnrealizedProfitPercentPerSecurity import MaximumUnrealizedProfitPercentPerSecurity

### <summary>
### Regression algorithm to assert the behavior of <see cref="MaximumUnrealizedProfitPercentPerSecurity"/>.
### </summary>
class MaximumUnrealizedProfitPercentPerSecurityFrameworkRegressionAlgorithm(BaseFrameworkRegressionAlgorithm):

    def initialize(self):
        super().initialize()
        self.set_universe_selection(ManualUniverseSelectionModel(Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)))

        self.set_risk_management(MaximumUnrealizedProfitPercentPerSecurity(0.004))
