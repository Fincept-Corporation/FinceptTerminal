# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-203BF5C3
# Category: General Strategy
# Description: Basic template algorithm that implements a fill model with combo orders
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Basic template algorithm that implements a fill model with combo orders
### <meta name="tag" content="trading and orders" />
### </summary>
class ComboOrdersFillModelAlgorithm(QCAlgorithm):
    '''Basic template algorithm that implements a fill model with combo orders'''

    def initialize(self):
        self.set_start_date(2019, 1, 1)
        self.set_end_date(2019, 1, 20)

        self.spy = self.add_equity("SPY", Resolution.HOUR)
        self.ibm = self.add_equity("IBM", Resolution.HOUR)

        # Set the fill model
        self.spy.set_fill_model(CustomPartialFillModel())
        self.ibm.set_fill_model(CustomPartialFillModel())

        self.order_types = {}

    def on_data(self, data):
        if not self.portfolio.invested:
            legs = [Leg.create(self.spy.symbol, 1), Leg.create(self.ibm.symbol, -1)]
            self.combo_market_order(legs, 100)
            self.combo_limit_order(legs, 100, round(self.spy.bid_price))

            legs = [Leg.create(self.spy.symbol, 1, round(self.spy.bid_price) + 1), Leg.create(self.ibm.symbol, -1, round(self.ibm.bid_price) + 1)]
            self.combo_leg_limit_order(legs, 100)

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            order_type = self.transactions.get_order_by_id(order_event.order_id).type
            if order_type == OrderType.COMBO_MARKET and order_event.absolute_fill_quantity != 50:
                raise Exception(f"The absolute quantity filled for all combo market orders should be 50, but for order {order_event.order_id} was {order_event.absolute_fill_quantity}")
            elif order_type == OrderType.COMBO_LIMIT and order_event.absolute_fill_quantity != 20:
                raise Exception(f"The absolute quantity filled for all combo limit orders should be 20, but for order {order_event.order_id} was {order_event.absolute_fill_quantity}")
            elif order_type == OrderType.COMBO_LEG_LIMIT and order_event.absolute_fill_quantity != 10:
                raise Exception(f"The absolute quantity filled for all combo leg limit orders should be 10, but for order {order_event.order_id} was {order_event.absolute_fill_quantity}")

            self.order_types[order_type] = 1

    def on_end_of_algorithm(self):
        if len(self.order_types) != 3:
            raise Exception(f"Just 3 different types of order were submitted in this algorithm, but the amount of order types was {len(self.order_types)}")

        if OrderType.COMBO_MARKET not in self.order_types.keys():
            raise Exception(f"One Combo Market Order should have been submitted but it was not")

        if OrderType.COMBO_LIMIT not in self.order_types.keys():
            raise Exception(f"One Combo Limit Order should have been submitted but it was not")

        if OrderType.COMBO_LEG_LIMIT not in self.order_types.keys():
            raise Exception(f"One Combo Leg Limit Order should have been submitted but it was not")


class CustomPartialFillModel(FillModel):
    '''Implements a custom fill model that inherit from FillModel. Overrides combo_market_fill, combo_limit_fill and combo_leg_limit_fill
       methods to test FillModelPythonWrapper works as expected'''

    def __init__(self):
        self.absolute_remaining_by_order_id = {}

    def fill_orders_partially(self, parameters, fills, quantity):
        partial_fills = []
        if len(fills) == 0:
            return partial_fills

        for kvp, fill in zip(sorted(parameters.securities_for_orders, key=lambda x: x.key.id), fills):
            order = kvp.key

            absolute_remaining = self.absolute_remaining_by_order_id.get(order.id, order.absolute_quantity)

            # Set the fill amount
            fill.fill_quantity = np.sign(order.quantity) * quantity

            if (min(abs(fill.fill_quantity), absolute_remaining) == absolute_remaining):
                fill.fill_quantity = np.sign(order.quantity) * absolute_remaining
                fill.status = OrderStatus.FILLED
                self.absolute_remaining_by_order_id.pop(order.id, None)
            else:
                fill.status = OrderStatus.PARTIALLY_FILLED
                self.absolute_remaining_by_order_id[order.id] = absolute_remaining - abs(fill.fill_quantity)
                price = fill.fill_price
                # self.algorithm.debug(f"{self.algorithm.time} - Partial Fill - Remaining {self.absolute_remaining_by_order_id[order.id]} Price - {price}")

            partial_fills.append(fill)

        return partial_fills

    def combo_market_fill(self, order, parameters):
        fills = super().combo_market_fill(order, parameters)
        partial_fills = self.fill_orders_partially(parameters, fills, 50)
        return partial_fills

    def combo_limit_fill(self, order, parameters):
        fills = super().combo_limit_fill(order, parameters)
        partial_fills = self.fill_orders_partially(parameters, fills, 20)
        return partial_fills

    def combo_leg_limit_fill(self, order, parameters):
        fills = super().combo_leg_limit_fill(order, parameters)
        partial_fills = self.fill_orders_partially(parameters, fills, 10)
        return partial_fills
