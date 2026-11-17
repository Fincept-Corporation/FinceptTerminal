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
### Basic Template India Index Algorithm uses framework components to define the algorithm.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class BasicTemplateIndiaIndexAlgorithm(QCAlgorithm):
    '''Basic template framework algorithm uses framework components to define the algorithm.'''

    def initialize(self):
        '''initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_account_currency("INR") #Set Account Currency
        self.set_start_date(2019, 1, 1)  #Set Start Date
        self.set_end_date(2019, 1, 5)    #Set End Date
        self.set_cash(1000000)          #Set Strategy Cash

        # Use indicator for signal; but it cannot be traded
        self.nifty = self.add_index("NIFTY50", Resolution.MINUTE, Market.INDIA).symbol
        # Trade Index based ETF
        self.nifty_etf = self.add_equity("JUNIORBEES", Resolution.MINUTE, Market.INDIA).symbol
   
        # Set Order Properties as per the requirements for order placement
        self.default_order_properties = IndiaOrderProperties(Exchange.NSE)

        # Define indicator
        self._ema_slow = self.ema(self.nifty, 80)
        self._ema_fast = self.ema(self.nifty, 200)

        self.debug("numpy test >>> print numpy.pi: " + str(np.pi))


    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''

        if not data.bars.contains_key(self.nifty) or not data.bars.contains_key(self.nifty_etf):
            return

        if not self._ema_slow.is_ready:
            return

        if self._ema_fast > self._ema_slow:
            if not self.portfolio.invested:
                self.market_ticket = self.market_order(self.nifty_etf, 1)
        else:
            self.liquidate()


    def on_end_of_algorithm(self):
        if self.portfolio[self.nifty].total_sale_volume > 0:
            raise Exception("Index is not tradable.")

