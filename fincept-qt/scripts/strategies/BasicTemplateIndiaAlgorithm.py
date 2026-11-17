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
class BasicTemplateIndiaAlgorithm(QCAlgorithm):
    '''Basic template framework algorithm uses framework components to define the algorithm.'''

    def initialize(self):
        '''initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_account_currency("INR")  #Set Account Currency 
        self.set_start_date(2019, 1, 23)  #Set Start Date
        self.set_end_date(2019, 10, 31)   #Set End Date
        self.set_cash(100000)            #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("YESBANK", Resolution.MINUTE, Market.INDIA)
        self.debug("numpy test >>> print numpy.pi: " + str(np.pi))

        # Set Order Properties as per the requirements for order placement
        self.default_order_properties = IndiaOrderProperties(Exchange.NSE)

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.market_order("YESBANK", 1)

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug("Purchased Stock: {0}".format(order_event.symbol))
