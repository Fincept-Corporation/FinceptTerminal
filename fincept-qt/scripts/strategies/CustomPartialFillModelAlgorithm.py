# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3E3DC670
# Category: General Strategy
# Description: Basic template algorithm that implements a fill model with partial fills
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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
