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
# Modified by Fincept Corporation for the Fincept Terminal: Python 3 syntax fix (date literal 07 -> 7) and whitespace.

from AlgorithmImports import *

### <summary>
### Example algorithm to demostrate the event handlers of Brokerage activities
### </summary>
### <meta name="tag" content="using quantconnect" />
class BrokerageActivityEventHandlingAlgorithm(QCAlgorithm):
    
    ### <summary>
    ### Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    ### </summary>
    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)

        self.add_equity("SPY", Resolution.MINUTE)

    ### <summary>
    ### on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    ### </summary>
    ### <param name="data">Slice object keyed by symbol containing the stock data</param>
    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)

    ### <summary>
    ### Brokerage message event handler. This method is called for all types of brokerage messages.
    ### </summary>
    def on_brokerage_message(self, message_event):
        self.debug(f"Brokerage meesage received - {message_event.to_string()}")

    ### <summary>
    ### Brokerage disconnected event handler. This method is called when the brokerage connection is lost.
    ### </summary>
    def on_brokerage_disconnect(self):
        self.debug(f"Brokerage disconnected!")
    
    ### <summary>
    ### Brokerage reconnected event handler. This method is called when the brokerage connection is restored after a disconnection.
    ### </summary>
    def on_brokerage_reconnect(self):
        self.debug(f"Brokerage reconnected!")
