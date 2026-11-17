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
### Uses daily data and a simple moving average cross to place trades and an ema for stop placement
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="trading and orders" />
class DailyAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,1,1)    #Set Start Date
        self.set_end_date(2014,1,1)      #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY", Resolution.DAILY)
        self.add_equity("IBM", Resolution.HOUR).set_leverage(1.0)
        self.macd = self.macd("SPY", 12, 26, 9, MovingAverageType.WILDERS, Resolution.DAILY, Field.CLOSE)
        self.ema = self.ema("IBM", 15 * 6, Resolution.HOUR, Field.SEVEN_BAR)
        self.last_action = None


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.macd.is_ready: return
        if not data.contains_key("IBM"): return
        if data["IBM"] is None:
            self.log("Price Missing Time: %s"%str(self.time))
            return
        if self.last_action is not None and self.last_action.date() == self.time.date(): return

        self.last_action = self.time
        quantity = self.portfolio["SPY"].quantity

        if quantity <= 0 and self.macd.current.value > self.macd.signal.current.value and data["IBM"].price > self.ema.current.value:
            self.set_holdings("IBM", 0.25)
        elif quantity >= 0 and self.macd.current.value < self.macd.signal.current.value and data["IBM"].price < self.ema.current.value:
            self.set_holdings("IBM", -0.25)
