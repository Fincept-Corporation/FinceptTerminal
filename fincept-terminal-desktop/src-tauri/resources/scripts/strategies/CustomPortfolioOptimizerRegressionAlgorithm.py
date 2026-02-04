# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9C9C9891
# Category: Portfolio Management
# Description: Regression algorithm asserting we can specify a custom portfolio optimizer with a MeanVarianceOptimizationPortfolioCo...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from MeanVarianceOptimizationFrameworkAlgorithm import MeanVarianceOptimizationFrameworkAlgorithm

### <summary>
### Regression algorithm asserting we can specify a custom portfolio
### optimizer with a MeanVarianceOptimizationPortfolioConstructionModel
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class CustomPortfolioOptimizerRegressionAlgorithm(MeanVarianceOptimizationFrameworkAlgorithm):
    def initialize(self):
        super().initialize()
        self.set_portfolio_construction(MeanVarianceOptimizationPortfolioConstructionModel(timedelta(days=1), PortfolioBias.LONG_SHORT, 1, 63, Resolution.DAILY, 0.02, CustomPortfolioOptimizer()))

class CustomPortfolioOptimizer:
    def optimize(self, historical_returns, expected_returns, covariance):
        return [0.5]*(np.array(historical_returns)).shape[1]
