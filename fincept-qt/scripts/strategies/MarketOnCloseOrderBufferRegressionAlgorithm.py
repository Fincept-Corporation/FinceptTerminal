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

class MarketOnCloseOrderBufferRegressionAlgorithm(QCAlgorithm):
    valid_order_ticket = None
    invalid_order_ticket = None

    def initialize(self):
        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,8)    #Set End Date

        self.add_equity("SPY", Resolution.MINUTE)

        def moc_at_post_market():
            self.valid_order_ticket_extended_market_hours = self.market_on_close_order("SPY", 2)

        self.schedule.on(self.date_rules.today, self.time_rules.at(17,0), moc_at_post_market)
                
        # Modify our submission buffer time to 10 minutes
        MarketOnCloseOrder.submission_time_buffer = timedelta(minutes=10)

    def on_data(self, data):
        # Test our ability to submit MarketOnCloseOrders
        # Because we set our buffer to 10 minutes, any order placed
        # before 3:50PM should be accepted, any after marked invalid

        # Will not throw an order error and execute
        if self.time.hour == 15 and self.time.minute == 49 and not self.valid_order_ticket:
            self.valid_order_ticket = self.market_on_close_order("SPY", 2)

        # Will throw an order error and be marked invalid
        if self.time.hour == 15 and self.time.minute == 51 and not self.invalid_order_ticket:
            self.invalid_order_ticket = self.market_on_close_order("SPY", 2)

    def on_end_of_algorithm(self):
        # Set it back to default for other regressions
        MarketOnCloseOrder.submission_time_buffer = MarketOnCloseOrder.DEFAULT_SUBMISSION_TIME_BUFFER

        if self.valid_order_ticket.status != OrderStatus.FILLED:
            raise Exception("Valid order failed to fill")

        if self.invalid_order_ticket.status != OrderStatus.INVALID:
            raise Exception("Invalid order was not rejected")

        if self.valid_order_ticket_extended_market_hours.status != OrderStatus.FILLED:
            raise Exception("Valid order during extended market hours failed to fill")
