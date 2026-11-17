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

# <summary>
# Regression algorithm to test the behaviour of ARMA versus AR models at the same order of differencing.
# In particular, an ARIMA(1,1,1) and ARIMA(1,1,0) are instantiated while orders are placed if their difference
# is sufficiently large (which would be due to the inclusion of the MA(1) term).
# </summary>
class AutoRegressiveIntegratedMovingAverageRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2013, 1, 7)
        self.set_end_date(2013, 12, 11)
        self.settings.automatic_indicator_warm_up = True
        self.add_equity("SPY", Resolution.DAILY)
        self._arima = self.arima("SPY", 1, 1, 1, 50)
        self._ar = self.arima("SPY", 1, 1, 0, 50)

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if self._arima.is_ready:
            if abs(self._arima.current.value - self._ar.current.value) > 1:
                if self._arima.current.value > self.last:
                    self.market_order("SPY", 1)
                else:
                    self.market_order("SPY", -1)
            self.last = self._arima.current.value
