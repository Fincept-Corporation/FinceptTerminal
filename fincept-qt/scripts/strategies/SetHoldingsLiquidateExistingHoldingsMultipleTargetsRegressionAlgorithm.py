# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-36FF2819
# Category: Regression Test
# Description: Regression algorithm testing GH feature 3790, using SetHoldings with a collection of targets which will be ordered by...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from SetHoldingsMultipleTargetsRegressionAlgorithm import SetHoldingsMultipleTargetsRegressionAlgorithm

### <summary>
### Regression algorithm testing GH feature 3790, using SetHoldings with a collection of targets
### which will be ordered by margin impact before being executed, with the objective of avoiding any
### margin errors
### Asserts that liquidate_existing_holdings equal false does not close positions inadvertedly (GH 7008) 
### </summary>
class SetHoldingsLiquidateExistingHoldingsMultipleTargetsRegressionAlgorithm(SetHoldingsMultipleTargetsRegressionAlgorithm):
    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings([PortfolioTarget(self._spy, 0.8), PortfolioTarget(self._ibm, 0.2)],
                             liquidate_existing_holdings=True)
