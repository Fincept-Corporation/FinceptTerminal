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

from Alphas.HistoricalReturnsAlphaModel import HistoricalReturnsAlphaModel
from Portfolio.BlackLittermanOptimizationPortfolioConstructionModel import *
from Portfolio.UnconstrainedMeanVariancePortfolioOptimizer import UnconstrainedMeanVariancePortfolioOptimizer
from Risk.NullRiskManagementModel import NullRiskManagementModel

### <summary>
### Black-Litterman framework algorithm
### Uses the HistoricalReturnsAlphaModel and the BlackLittermanPortfolioConstructionModel
### to create an algorithm that rebalances the portfolio according to Black-Litterman portfolio optimization
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class BlackLittermanPortfolioOptimizationFrameworkAlgorithm(QCAlgorithm):
    '''Black-Litterman Optimization algorithm.'''

    def initialize(self):

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self._symbols = [ Symbol.create(x, SecurityType.EQUITY, Market.USA) for x in [ 'AIG', 'BAC', 'IBM', 'SPY' ] ]

        optimizer = UnconstrainedMeanVariancePortfolioOptimizer()

        # set algorithm framework models
        self.set_universe_selection(CoarseFundamentalUniverseSelectionModel(self.coarse_selector))
        self.set_alpha(HistoricalReturnsAlphaModel(resolution = Resolution.DAILY))
        self.set_portfolio_construction(BlackLittermanOptimizationPortfolioConstructionModel(optimizer = optimizer))
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())

    def coarse_selector(self, coarse):
        # Drops SPY after the 8th
        last = 3 if self.time.day > 8 else len(self._symbols)

        return self._symbols[0:last]

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug(order_event)
