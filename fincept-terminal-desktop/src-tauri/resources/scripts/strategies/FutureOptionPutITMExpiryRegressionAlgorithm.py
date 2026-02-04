# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-88F9D2E8
# Category: Options
# Description: This regression algorithm tests In The Money (ITM) future option expiry for puts. We expect 3 orders from the algorit...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests In The Money (ITM) future option expiry for puts.
### We expect 3 orders from the algorithm, which are:
###
###   * Initial entry, buy ES Put Option (expiring ITM) (buy, qty 1)
###   * Option exercise, receiving short ES future contracts (sell, qty -1)
###   * Future contract liquidation, due to impending expiry (buy qty 1)
###
### Additionally, we test delistings for future options and assert that our
### portfolio holdings reflect the orders the algorithm has submitted.
### </summary>
class FutureOptionPutITMExpiryRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2020, 1, 5)
        self.set_end_date(2020, 6, 30)

        self.es19m20 = self.add_future_contract(
            Symbol.create_future(
                Futures.Indices.SP_500_E_MINI,
                Market.CME,
                datetime(2020, 6, 19)
            ),
            Resolution.MINUTE).symbol

        # Select a future option expiring ITM, and adds it to the algorithm.
        self.es_option = self.add_future_option_contract(
            list(
                sorted([x for x in self.option_chain(self.es19m20) if x.id.strike_price >= 3300.0 and x.id.option_right == OptionRight.PUT],
                       key=lambda x: x.id.strike_price)
            )[0], Resolution.MINUTE).symbol

        self.expected_contract = Symbol.create_option(self.es19m20, Market.CME, OptionStyle.AMERICAN, OptionRight.PUT, 3300.0, datetime(2020, 6, 19))
        if self.es_option != self.expected_contract:
            raise AssertionError(f"Contract {self.expected_contract} was not found in the chain")

        self.schedule.on(self.date_rules.tomorrow, self.time_rules.after_market_open(self.es19m20, 1), self.schedule_callback)

    def schedule_callback(self):
        self.market_order(self.es_option, 1)

    def on_data(self, data: Slice):
        # Assert delistings, so that we can make sure that we receive the delisting warnings at
        # the expected time. These assertions detect bug #4872
        for delisting in data.delistings.values():
            if delisting.type == DelistingType.WARNING:
                if delisting.time != datetime(2020, 6, 19):
                    raise AssertionError(f"Delisting warning issued at unexpected date: {delisting.time}")
            elif delisting.type == DelistingType.DELISTED:
                if delisting.time != datetime(2020, 6, 20):
                    raise AssertionError(f"Delisting happened at unexpected date: {delisting.time}")

    def on_order_event(self, order_event: OrderEvent):
        if order_event.status != OrderStatus.FILLED:
            # There's lots of noise with OnOrderEvent, but we're only interested in fills.
            return

        if not self.securities.contains_key(order_event.symbol):
            raise AssertionError(f"Order event Symbol not found in Securities collection: {order_event.symbol}")

        security = self.securities[order_event.symbol]
        if security.symbol == self.es19m20:
            self.assert_future_option_order_exercise(order_event, security, self.securities[self.expected_contract])
        elif security.symbol == self.expected_contract:
            # Expected contract is ES19M20 Call Option expiring ITM @ 3250
            self.assert_future_option_contract_order(order_event, security)
        else:
            raise AssertionError(f"Received order event for unknown Symbol: {order_event.symbol}")

        self.log(f"{self.time} -- {order_event.symbol} :: Price: {self.securities[order_event.symbol].holdings.price} Qty: {self.securities[order_event.symbol].holdings.quantity} Direction: {order_event.direction} Msg: {order_event.message}")

    def assert_future_option_order_exercise(self, order_event: OrderEvent, future: Security, option_contract: Security):
        expected_liquidation_time_utc = datetime(2020, 6, 20, 4, 0, 0)

        if order_event.direction == OrderDirection.BUY and future.holdings.quantity != 0:
            # We expect the contract to have been liquidated immediately
            raise AssertionError(f"Did not liquidate existing holdings for Symbol {future.symbol}")
        if order_event.direction == OrderDirection.BUY and order_event.utc_time.replace(tzinfo=None) != expected_liquidation_time_utc:
            raise AssertionError(f"Liquidated future contract, but not at the expected time. Expected: {expected_liquidation_time_utc} - found {order_event.utc_time.replace(tzinfo=None)}")

        # No way to detect option exercise orders or any other kind of special orders
        # other than matching strings, for now.
        if "Option Exercise" in order_event.message:
            if order_event.fill_price != 3300.0:
                raise AssertionError("Option did not exercise at expected strike price (3300)")

            if future.holdings.quantity != -1:
                # Here, we expect to have some holdings in the underlying, but not in the future option anymore.
                raise AssertionError(f"Exercised option contract, but we have no holdings for Future {future.symbol}")

            if option_contract.holdings.quantity != 0:
                raise AssertionError(f"Exercised option contract, but we have holdings for Option contract {option_contract.symbol}")

    def assert_future_option_contract_order(self, order_event: OrderEvent, option: Security):
        if order_event.direction == OrderDirection.BUY and option.holdings.quantity != 1:
            raise AssertionError(f"No holdings were created for option contract {option.symbol}")

        if order_event.direction == OrderDirection.SELL and option.holdings.quantity != 0:
            raise AssertionError(f"Holdings were found after a filled option exercise")

        if "Exercise" in order_event.message and option.holdings.quantity != 0:
            raise AssertionError(f"Holdings were found after exercising option contract {option.symbol}")

    def on_end_of_algorithm(self):
        if self.portfolio.invested:
            raise AssertionError(f"Expected no holdings at end of algorithm, but are invested in: {', '.join([str(i.id) for i in self.portfolio.keys()])}")
