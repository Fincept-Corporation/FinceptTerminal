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

trade_flag = False

### <summary>
### Regression algorithm asserting slice.get() works for both the alpha and the algorithm
### </summary>
class SliceGetByTypeRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10,11)

        self.add_equity("SPY", Resolution.MINUTE)
        self.set_alpha(TestAlphaModel())

    def on_data(self, data):
        if "SPY" in data:
            tb = data.get(TradeBar)["SPY"]
            global trade_flag
            if not self.portfolio.invested and trade_flag:
                self.set_holdings("SPY", 1)

class TestAlphaModel(AlphaModel):
    def update(self, algorithm, data):
        insights = []

        if "SPY" in data:
            tb = data.get(TradeBar)["SPY"]
            global trade_flag
            trade_flag = True

        return insights

    def on_securities_changed(self, algorithm, changes):
        return
