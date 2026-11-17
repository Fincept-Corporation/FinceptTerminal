# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

from AlgorithmImports import *

### <summary>
### This regression algorithm tests Out of The Money (OTM) index option expiry for short calls.
### We expect 2 orders from the algorithm, which are:
###
###   * Initial entry, sell SPX Call Option (expiring OTM)
###     - Profit the option premium, since the option was not assigned.
###
###   * Liquidation of SPX call OTM contract on the last trade date
###
### Additionally, we test delistings for index options and assert that our
### portfolio holdings reflect the orders the algorithm has submitted.
### </summary>
class IndexOptionShortCallOTMExpiryRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 1, 31)

        self.spx = self.add_index("SPX", Resolution.MINUTE).symbol

        # Select a index option expiring ITM, and adds it to the algorithm.
        self.spx_option = list(self.option_chain(self.spx))
        self.spx_option = [i for i in self.spx_option if i.id.strike_price >= 4250 and i.id.option_right == OptionRight.CALL and i.id.date.year == 2021 and i.id.date.month == 1]
        self.spx_option = list(sorted(self.spx_option, key=lambda x: x.id.strike_price))[0]
        self.spx_option = self.add_index_option_contract(self.spx_option, Resolution.MINUTE).symbol

        self.expected_contract = Symbol.create_option(self.spx, Market.USA, OptionStyle.EUROPEAN, OptionRight.CALL, 4250, datetime(2021, 1, 15))
        if self.spx_option != self.expected_contract:
            raise Exception(f"Contract {self.expected_contract} was not found in the chain")

        self.schedule.on(self.date_rules.tomorrow, self.time_rules.after_market_open(self.spx, 1), lambda: self.market_order(self.spx_option, -1))

    def on_data(self, data: Slice):
        # Assert delistings, so that we can make sure that we receive the delisting warnings at
        # the expected time. These assertions detect bug #4872
        for delisting in data.delistings.values():
            if delisting.type == DelistingType.WARNING:
                if delisting.time != datetime(2021, 1, 15):
                    raise Exception(f"Delisting warning issued at unexpected date: {delisting.time}")

            if delisting.type == DelistingType.DELISTED:
                if delisting.time != datetime(2021, 1, 16):
                    raise Exception(f"Delisting happened at unexpected date: {delisting.time}")

    def on_order_event(self, order_event: OrderEvent):
        if order_event.status != OrderStatus.FILLED:
            # There's lots of noise with OnOrderEvent, but we're only interested in fills.
            return

        if order_event.symbol not in self.securities:
            raise Exception(f"Order event Symbol not found in Securities collection: {order_event.symbol}")

        security = self.securities[order_event.symbol]
        if security.symbol == self.spx:
            raise Exception(f"Expected no order events for underlying Symbol {security.symbol}")

        if security.symbol == self.expected_contract:
            self.assert_index_option_contract_order(order_event, security)
        else:
            raise Exception(f"Received order event for unknown Symbol: {order_event.symbol}")

    def assert_index_option_contract_order(self, order_event: OrderEvent, option_contract: Security):
        if order_event.direction == OrderDirection.SELL and option_contract.holdings.quantity != -1:
            raise Exception(f"No holdings were created for option contract {option_contract.symbol}")
        if order_event.direction == OrderDirection.BUY and option_contract.holdings.quantity != 0:
            raise Exception("Expected no options holdings after closing position")
        if order_event.is_assignment:
            raise Exception(f"Assignment was not expected for {order_event.symbol}")

    ### <summary>
    ### Ran at the end of the algorithm to ensure the algorithm has no holdings
    ### </summary>
    ### <exception cref="Exception">The algorithm has holdings</exception>
    def on_end_of_algorithm(self):
        if self.portfolio.invested:
            raise Exception(f"Expected no holdings at end of algorithm, but are invested in: {', '.join(self.portfolio.keys())}")
