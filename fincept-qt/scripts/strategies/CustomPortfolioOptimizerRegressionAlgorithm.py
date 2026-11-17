# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

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
