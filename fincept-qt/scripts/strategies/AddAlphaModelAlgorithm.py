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

### <summary>
### Test algorithm using 'QCAlgorithm.add_alpha_model()'
### </summary>
class AddAlphaModelAlgorithm(QCAlgorithm):
    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self.universe_settings.resolution = Resolution.DAILY

        spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        fb = Symbol.create("FB", SecurityType.EQUITY, Market.USA)
        ibm = Symbol.create("IBM", SecurityType.EQUITY, Market.USA)

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel([ spy, fb, ibm ]))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

        self.add_alpha(OneTimeAlphaModel(spy))
        self.add_alpha(OneTimeAlphaModel(fb))
        self.add_alpha(OneTimeAlphaModel(ibm))

class OneTimeAlphaModel(AlphaModel):
    def __init__(self, symbol):
        self.symbol = symbol
        self.triggered = False

    def update(self, algorithm, data):
        insights = []
        if not self.triggered:
            self.triggered = True
            insights.append(Insight.price(
                self.symbol,
                Resolution.DAILY,
                1,
                InsightDirection.DOWN))
        return insights
