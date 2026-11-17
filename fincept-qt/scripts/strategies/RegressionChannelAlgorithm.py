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
### Regression Channel algorithm simply initializes the date range and cash
### </summary>
### <meta name="tag" content="indicators" />
### <meta name="tag" content="indicator classes" />
### <meta name="tag" content="placing orders" />
### <meta name="tag" content="plotting indicators" />
class RegressionChannelAlgorithm(QCAlgorithm):

    def initialize(self):

        self.set_cash(100000)
        self.set_start_date(2009,1,1)
        self.set_end_date(2015,1,1)

        equity = self.add_equity("SPY", Resolution.MINUTE)
        self._spy = equity.symbol
        self._holdings = equity.holdings
        self._rc = self.rc(self._spy, 30, 2, Resolution.DAILY)

        stock_plot = Chart("Trade Plot")
        stock_plot.add_series(Series("Buy", SeriesType.SCATTER, 0))
        stock_plot.add_series(Series("Sell", SeriesType.SCATTER, 0))
        stock_plot.add_series(Series("UpperChannel", SeriesType.LINE, 0))
        stock_plot.add_series(Series("LowerChannel", SeriesType.LINE, 0))
        stock_plot.add_series(Series("Regression", SeriesType.LINE, 0))
        self.add_chart(stock_plot)

    def on_data(self, data):
        if (not self._rc.is_ready) or (not data.contains_key(self._spy)): return
        if data[self._spy] is None: return
        value = data[self._spy].value
        if self._holdings.quantity <= 0 and value < self._rc.lower_channel.current.value:
            self.set_holdings(self._spy, 1)
            self.plot("Trade Plot", "Buy", value)
        if self._holdings.quantity >= 0 and value > self._rc.upper_channel.current.value:
            self.set_holdings(self._spy, -1)
            self.plot("Trade Plot", "Sell", value)

    def on_end_of_day(self, symbol):
        self.plot("Trade Plot", "UpperChannel", self._rc.upper_channel.current.value)
        self.plot("Trade Plot", "LowerChannel", self._rc.lower_channel.current.value)
        self.plot("Trade Plot", "Regression", self._rc.linear_regression.current.value)
