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
### This algorithm demonstrates the runtime addition and removal of securities from your algorithm.
### With LEAN it is possible to add and remove securities after the initialization.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="assets" />
### <meta name="tag" content="regression test" />
class AddRemoveSecurityRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY")

        self._last_action = None


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if self._last_action is not None and self._last_action.date() == self.time.date():
            return

        if not self.portfolio.invested:
            self.set_holdings("SPY", .5)
            self._last_action = self.time

        if self.time.weekday() == 1:
            self.add_equity("AIG")
            self.add_equity("BAC")
            self._last_action = self.time

        if self.time.weekday() == 2:
            self.set_holdings("AIG", .25)
            self.set_holdings("BAC", .25)
            self._last_action = self.time

        if self.time.weekday() == 3:
            self.remove_security("AIG")
            self.remove_security("BAC")
            self._last_action = self.time

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.SUBMITTED:
            self.debug("{0}: Submitted: {1}".format(self.time, self.transactions.get_order_by_id(order_event.order_id)))
        if order_event.status == OrderStatus.FILLED:
            self.debug("{0}: Filled: {1}".format(self.time, self.transactions.get_order_by_id(order_event.order_id)))
