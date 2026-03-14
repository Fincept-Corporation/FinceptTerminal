# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3555D255
# Category: General Strategy
# Description: Demonstration of using custom fee, slippage, fill, and buying power models for modeling transactions in backtesting. ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
import random

### <summary>
### Demonstration of using custom fee, slippage, fill, and buying power models for modeling transactions in backtesting.
### QuantConnect allows you to model all orders as deeply and accurately as you need.
### This example illustrates how Lean exports its API to Python conforming to PEP8 style guide.
### </summary>
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="transaction fees and slippage" />
### <meta name="tag" content="custom buying power models" />
### <meta name="tag" content="custom transaction models" />
### <meta name="tag" content="custom slippage models" />
### <meta name="tag" content="custom fee models" />
class CustomModelsPEP8Algorithm(QCAlgorithm):
    '''Demonstration of using custom fee, slippage, fill, and buying power models for modeling transactions in backtesting.
    QuantConnect allows you to model all orders as deeply and accurately as you need.'''

    def initialize(self):
        self.set_start_date(2013,10,1)   # Set Start Date
        self.set_end_date(2013,10,31)    # Set End Date
        self.security = self.add_equity("SPY", Resolution.HOUR)
        self.spy = self.security.symbol

        # set our models
        self.security.set_fee_model(CustomFeeModelPEP8(self))
        self.security.set_fill_model(CustomFillModelPEP8(self))
        self.security.set_slippage_model(CustomSlippageModelPEP8(self))
        self.security.set_buying_power_model(CustomBuyingPowerModelPEP8(self))

    def on_data(self, data):
        open_orders = self.transactions.get_open_orders(self.spy)
        if len(open_orders) != 0: return
        if self.time.day > 10 and self.security.holdings.quantity <= 0:
            quantity = self.calculate_order_quantity(self.spy, .5)
            self.log(f"MarketOrder: {quantity}")
            self.market_order(self.spy, quantity, True)   # async needed for partial fill market orders
        elif self.time.day > 20 and self.security.holdings.quantity >= 0:
            quantity = self.calculate_order_quantity(self.spy, -.5)
            self.log(f"MarketOrder: {quantity}")
            self.market_order(self.spy, quantity, True)   # async needed for partial fill market orders

class CustomFillModelPEP8(ImmediateFillModel):
    def __init__(self, algorithm):
        super().__init__()
        self.algorithm = algorithm
        self.absolute_remaining_by_order_id = {}
        self.random = Random(387510346)

    def market_fill(self, asset, order):
        absolute_remaining = order.absolute_quantity

        if order.id in self.absolute_remaining_by_order_id.keys():
            absolute_remaining = self.absolute_remaining_by_order_id[order.id]

        fill = super().market_fill(asset, order)
        absolute_fill_quantity = int(min(absolute_remaining, self.random.next(0, 2*int(order.absolute_quantity))))
        fill.fill_quantity = np.sign(order.quantity) * absolute_fill_quantity

        if absolute_remaining == absolute_fill_quantity:
            fill.status = OrderStatus.FILLED
            if self.absolute_remaining_by_order_id.get(order.id):
                self.absolute_remaining_by_order_id.pop(order.id)
        else:
            absolute_remaining = absolute_remaining - absolute_fill_quantity
            self.absolute_remaining_by_order_id[order.id] = absolute_remaining
            fill.status = OrderStatus.PARTIALLY_FILLED
        self.algorithm.log(f"CustomFillModel: {fill}")
        return fill

class CustomFeeModelPEP8(FeeModel):
    def __init__(self, algorithm):
        super().__init__()
        self.algorithm = algorithm

    def get_order_fee(self, parameters):
        # custom fee math
        fee = max(1, parameters.security.price
                  * parameters.order.absolute_quantity
                  * 0.00001)
        self.algorithm.log(f"CustomFeeModel: {fee}")
        return OrderFee(CashAmount(fee, "USD"))

class CustomSlippageModelPEP8:
    def __init__(self, algorithm):
        self.algorithm = algorithm

    def get_slippage_approximation(self, asset, order):
        # custom slippage math
        slippage = asset.price * 0.0001 * np.log10(2*float(order.absolute_quantity))
        self.algorithm.log(f"CustomSlippageModel: {slippage}")
        return slippage

class CustomBuyingPowerModelPEP8(BuyingPowerModel):
    def __init__(self, algorithm):
        super().__init__()
        self.algorithm = algorithm

    def has_sufficient_buying_power_for_order(self, parameters):
        # custom behavior: this model will assume that there is always enough buying power
        has_sufficient_buying_power_for_order_result = HasSufficientBuyingPowerForOrderResult(True)
        self.algorithm.log(f"CustomBuyingPowerModel: {has_sufficient_buying_power_for_order_result.is_sufficient}")
        return has_sufficient_buying_power_for_order_result

class SimpleCustomFillModelPEP8(FillModel):
    def __init__(self):
        super().__init()

    def _create_order_event(self, asset, order):
        utc_time = Extensions.convert_to_utc(asset.local_time, asset.exchange.time_zone)
        return OrderEvent(order, utc_time, OrderFee.ZERO)

    def _set_order_event_to_filled(self, fill, fill_price, fill_quantity):
        fill.status = OrderStatus.FILLED
        fill.fill_quantity = fill_quantity
        fill.fill_price = fill_price
        return fill

    def _get_trade_bar(self, asset, order_direction):
        trade_bar = asset.cache.get_data[TradeBar]()
        if trade_bar:
            return trade_bar

        price = asset.price
        return TradeBar(asset.local_time, asset.symbol, price, price, price, price, 0)

    def market_fill(self, asset, order):
        fill = self._create_order_event(asset, order)
        if order.status == OrderStatus.CANCELED:
            return fill

        fill_price = asset.cache.ask_price if order.direction == OrderDirection.BUY else asset.cache.bid_price
        return self._set_order_event_to_filled(fill, fill_price, order.quantity)

    def stop_market_fill(self, asset, order):
        fill = self._create_order_event(asset, order)
        if order.status == OrderStatus.CANCELED:
            return fill

        stop_price = order.stop_price
        trade_bar = self._get_trade_bar(asset, order.direction)

        if order.direction == OrderDirection.SELL and trade_bar.low < stop_price:
            return self._set_order_event_to_filled(fill, stop_price, order.quantity)

        if order.direction == OrderDirection.BUY and trade_bar.high > stop_price:
            return self._set_order_event_to_filled(fill, stop_price, order.quantity)

        return fill

    def limit_fill(self, asset, order):
        fill = self._create_order_event(asset, order)
        if order.status == OrderStatus.CANCELED:
            return fill

        limit_price = order.limit_price
        trade_bar = self._get_trade_bar(asset, order.direction)

        if order.direction == OrderDirection.SELL and trade_bar.high > limit_price:
            return self._set_order_event_to_filled(fill, limit_price, order.quantity)

        if order.direction == OrderDirection.BUY and trade_bar.low < limit_price:
            return self._set_order_event_to_filled(fill, limit_price, order.quantity)

        return fill
