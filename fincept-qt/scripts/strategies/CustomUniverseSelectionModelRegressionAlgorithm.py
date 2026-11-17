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
from Selection.FundamentalUniverseSelectionModel import FundamentalUniverseSelectionModel

### <summary>
### Regression algorithm showing how to implement a custom universe selection model and asserting it's behavior
### </summary>
class CustomUniverseSelectionModelRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014,3,24)
        self.set_end_date(2014,4,7)

        self.universe_settings.resolution = Resolution.DAILY
        self.set_universe_selection(CustomUniverseSelectionModel())

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            for kvp in self.active_securities:
                self.set_holdings(kvp.key, 0.1)

class CustomUniverseSelectionModel(FundamentalUniverseSelectionModel):

    def __init__(self, universe_settings = None):
        super().__init__(universe_settings)
        self._selected = False

    def select(self, algorithm, fundamental):
        if not self._selected:
            self._selected = True
            return [ Symbol.create('AAPL', SecurityType.EQUITY, Market.USA) ]
        return Universe.UNCHANGED
