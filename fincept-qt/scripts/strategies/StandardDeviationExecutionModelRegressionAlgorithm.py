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
from Alphas.RsiAlphaModel import RsiAlphaModel
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel
from Execution.StandardDeviationExecutionModel import StandardDeviationExecutionModel

### <summary>
### Regression algorithm for the StandardDeviationExecutionModel.
### This algorithm shows how the execution model works to split up orders and submit them
### only when the price is 2 standard deviations from the 60min mean (default model settings).
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class StandardDeviationExecutionModelRegressionAlgorithm(QCAlgorithm):
    '''Regression algorithm for the StandardDeviationExecutionModel.
    This algorithm shows how the execution model works to split up orders and submit them
    only when the price is 2 standard deviations from the 60min mean (default model settings).'''

    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)
        self.set_cash(1000000)

        self.set_universe_selection(ManualUniverseSelectionModel([
            Symbol.create('AIG', SecurityType.EQUITY, Market.USA),
            Symbol.create('BAC', SecurityType.EQUITY, Market.USA),
            Symbol.create('IBM', SecurityType.EQUITY, Market.USA),
            Symbol.create('SPY', SecurityType.EQUITY, Market.USA)
        ]))

        self.set_alpha(RsiAlphaModel(14, Resolution.HOUR))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(StandardDeviationExecutionModel())

    def on_order_event(self, order_event):
        self.log(f"{self.time}: {order_event}")
