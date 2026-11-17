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
### Basic template algorithm that implements a fill model with partial fills
### <meta name="tag" content="trading and orders" />
### </summary>
class CustomPartialFillModelAlgorithm(QCAlgorithm):
    '''Basic template algorithm that implements a fill model with partial fills'''

    def initialize(self):
        self.set_start_date(2019, 1, 1)
        self.set_end_date(2019, 3, 1)

        equity = self.add_equity("SPY", Resolution.HOUR)
        self.spy = equity.symbol
        self.holdings = equity.holdings

        # Set the fill model
        equity.set_fill_model(CustomPartialFillModel(self))


    def on_data(self, data):
        open_orders = self.transactions.get_open_orders(self.spy)
        if len(open_orders) != 0: return

        if self.time.day > 10 and self.holdings.quantity <= 0:
            self.market_order(self.spy, 105, True)

        elif self.time.day > 20 and self.holdings.quantity >= 0:
            self.market_order(self.spy, -100, True)


class CustomPartialFillModel(FillModel):
    '''Implements a custom fill model that inherit from FillModel. Override the MarketFill method to simulate partially fill orders'''

    def __init__(self, algorithm):
        self.algorithm = algorithm
        self.absolute_remaining_by_order_id = {}

    def market_fill(self, asset, order):
        absolute_remaining = self.absolute_remaining_by_order_id.get(order.id, order. AbsoluteQuantity)

        # Create the object
        fill = super().market_fill(asset, order)

        # Set the fill amount
        fill.fill_quantity = np.sign(order.quantity) * 10

        if (min(abs(fill.fill_quantity), absolute_remaining) == absolute_remaining):
            fill.fill_quantity = np.sign(order.quantity) * absolute_remaining
            fill.status = OrderStatus.FILLED
            self.absolute_remaining_by_order_id.pop(order.id, None)
        else:
            fill.status = OrderStatus.PARTIALLY_FILLED
            self.absolute_remaining_by_order_id[order.id] = absolute_remaining - abs(fill.fill_quantity)
            price = fill.fill_price
            # self.algorithm.debug(f"{self.algorithm.time} - Partial Fill - Remaining {self.absolute_remaining_by_order_id[order.id]} Price - {price}")

        return fill
