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
### Algorithm used for regression tests purposes
### </summary>
### <meta name="tag" content="regression test" />
class RegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(10000000)         #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY", Resolution.TICK)
        self.add_equity("BAC", Resolution.MINUTE)
        self.add_equity("AIG", Resolution.HOUR)
        self.add_equity("IBM", Resolution.DAILY)

        self.__last_trade_ticks = self.start_date
        self.__last_trade_trade_bars = self.__last_trade_ticks
        self.__trade_every = timedelta(minutes=1)


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if self.time - self.__last_trade_trade_bars < self.__trade_every:
            return
        self.__last_trade_trade_bars = self.time

        for kvp in data.bars:
            bar = kvp.Value
            if bar.is_fill_forward:
                continue

            symbol = kvp.key
            holdings = self.portfolio[symbol]

            if not holdings.invested:
                self.market_order(symbol, 10)
            else:
                self.market_order(symbol, -holdings.quantity)
