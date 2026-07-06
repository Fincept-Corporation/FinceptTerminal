# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-791ECD54
# Category: Regression Test
# Description: Regression algorithm to assert the behavior of <see cref="MaximumDrawdownPercentPerSecurity"/>
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from BaseFrameworkRegressionAlgorithm import BaseFrameworkRegressionAlgorithm
from Risk.MaximumDrawdownPercentPerSecurity import MaximumDrawdownPercentPerSecurity

### <summary>
### Regression algorithm to assert the behavior of <see cref="MaximumDrawdownPercentPerSecurity"/>.
### </summary>
class MaximumDrawdownPercentPerSecurityFrameworkRegressionAlgorithm(BaseFrameworkRegressionAlgorithm):

    def initialize(self):
        super().initialize()
        self.set_universe_selection(ManualUniverseSelectionModel(Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)))

        self.set_risk_management(MaximumDrawdownPercentPerSecurity(0.004))

