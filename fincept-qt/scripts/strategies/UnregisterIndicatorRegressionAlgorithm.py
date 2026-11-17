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
### Example and regression algorithm asserting the behavior of registering and unregistering an indicator from the engine
### </summary>
class UnregisterIndicatorRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10,11)

        spy = self.add_equity("SPY")
        ibm = self.add_equity("IBM")

        self._symbols = [ spy.symbol, ibm.symbol ]
        self._trin = self.trin(self._symbols, Resolution.MINUTE)
        self._trin2 = None

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if self._trin.is_ready:
            self._trin.reset()
            self.unregister_indicator(self._trin)

            # let's create a new one with a differente resolution
            self._trin2 = self.trin(self._symbols, Resolution.HOUR)

        if not self._trin2 is None and self._trin2.is_ready:
            if self._trin.is_ready:
                raise ValueError("Indicator should of stop getting updates!")

            if not self.portfolio.invested:
                self.set_holdings(self._symbols[0], 0.5)
                self.set_holdings(self._symbols[1], 0.5)
