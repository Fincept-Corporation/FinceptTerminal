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
# Modified by Fincept Corporation for the Fincept Terminal: comment text changed; the code is unchanged.

from AlgorithmImports import *

### <summary>
### Basic template framework algorithm uses framework components to define the algorithm.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class BasicTemplateFrameworkAlgorithm(QCAlgorithm):
    '''Basic template framework algorithm uses framework components to define the algorithm.'''

    def initialize(self):
        '''initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        # Fincept Terminal Strategy Engine - Symbol Configuration
        # Forex, CFD, Equities Resolutions: Tick, Second, Minute, Hour, Daily.
        # Futures Resolution: Tick, Second, Minute
        # Options Resolution: Minute Only.
        symbols = [ Symbol.create("SPY", SecurityType.EQUITY, Market.USA) ]

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, None))

        # We can define how often the EWPCM will rebalance if no new insight is submitted using:
        # Resolution Enum:
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(Resolution.DAILY))
        # timedelta
        # self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(timedelta(2)))
        # A lamdda datetime -> datetime. In this case, we can use the pre-defined func at Expiry helper class
        # self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(Expiry.END_OF_WEEK))

        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(MaximumDrawdownPercentPerSecurity(0.01))

        self.debug("numpy test >>> print numpy.pi: " + str(np.pi))

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug("Purchased Stock: {0}".format(order_event.symbol))
