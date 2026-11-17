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
### Algorithm demonstrating the usage of custom brokerage message handler and the new brokerage-side order handling/filtering.
### This test is supposed to be ran by the CustomBrokerageMessageHandlerTests unit test fixture.
###
### All orders are sent from the brokerage, none of them will be placed by the algorithm.
### </summary>
class CustomBrokerageSideOrderHandlingRegressionAlgorithm(QCAlgorithm):
    '''Algorithm demonstrating the usage of custom brokerage message handler and the new brokerage-side order handling/filtering.
     This test is supposed to be ran by the CustomBrokerageMessageHandlerTests unit test fixture.

     All orders are sent from the brokerage, none of them will be placed by the algorithm.'''

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)

        self.set_brokerage_message_handler(CustomBrokerageMessageHandler(self))

        self._spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)

    def on_end_of_algorithm(self):
        # The security should have been added
        if not self.securities.contains_key(self._spy):
            raise Exception("Expected security to have been added")

        if self.transactions.orders_count == 0:
            raise Exception("Expected orders to be added from brokerage side")

        if len(list(self.portfolio.positions.groups)) != 1:
            raise Exception("Expected only one position")

class CustomBrokerageMessageHandler(DefaultBrokerageMessageHandler):
    def __init__(self, algorithm):
        self._algorithm = algorithm

    def handle_message(self, message):
        self._algorithm.debug(f"{self._algorithm.time} Event: {message.message}")

    def handle_order(self, event_args):
        order = event_args.order
        if order.tag is None or not order.tag.isdigit():
            raise Exception("Expected all new brokerage-side orders to have a valid tag")

        # We will only process orders with even tags
        return int(order.tag) % 2 == 0
