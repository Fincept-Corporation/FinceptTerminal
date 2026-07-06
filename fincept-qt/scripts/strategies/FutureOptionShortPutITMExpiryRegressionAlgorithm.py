# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-97BFC0D6
# Category: Options
# Description: This regression algorithm tests In The Money (ITM) future option expiry for short puts. We expect 3 orders from the a...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests In The Money (ITM) future option expiry for short puts.
### We expect 3 orders from the algorithm, which are:
###
###   * Initial entry, sell ES Put Option (expiring ITM)
###   * Option assignment, buy 1 contract of the underlying (ES)
###   * Future contract expiry, liquidation (sell 1 ES future)
###
### Additionally, we test delistings for future options and assert that our
### portfolio holdings reflect the orders the algorithm has submitted.
### </summary>
class FutureOptionShortPutITMExpiryRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 1, 5)
        self.set_end_date(2020, 6, 30)

        self.es19m20 = self.add_future_contract(
            Symbol.create_future(
                Futures.Indices.SP_500_E_MINI,
                Market.CME,
                datetime(2020, 6, 19)),
            Resolution.MINUTE).symbol

        # Select a future option expiring ITM, and adds it to the algorithm.
        self.es_option = self.add_future_option_contract(
            list(
                sorted(
                    [x for x in self.option_chain(self.es19m20) if x.id.strike_price <= 3400.0 and x.id.option_right == OptionRight.PUT],
                    key=lambda x: x.id.strike_price,
                    reverse=True
                )
            )[0], Resolution.MINUTE).symbol

        self.expected_contract = Symbol.create_option(self.es19m20, Market.CME, OptionStyle.AMERICAN, OptionRight.PUT, 3400.0, datetime(2020, 6, 19))
        if self.es_option != self.expected_contract:
            raise AssertionError(f"Contract {self.expected_contract} was not found in the chain")

        self.schedule.on(self.date_rules.tomorrow, self.time_rules.after_market_open(self.es19m20, 1), self.scheduled_market_order)

    def scheduled_market_order(self):
        self.market_order(self.es_option, -1)

    def on_data(self, data: Slice):
        # Assert delistings, so that we can make sure that we receive the delisting warnings at
        # the expected time. These assertions detect bug #4872
        for delisting in data.delistings.values():
            if delisting.type == DelistingType.WARNING:
                if delisting.time != datetime(2020, 6, 19):
                    raise AssertionError(f"Delisting warning issued at unexpected date: {delisting.time}")

            if delisting.type == DelistingType.DELISTED:
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
            self.assert_future_option_contract_order(order_event, security)

        else:
            raise AssertionError(f"Received order event for unknown Symbol: {order_event.symbol}")

        self.log(f"{order_event}")

    def assert_future_option_order_exercise(self, order_event: OrderEvent, future: Security, option_contract: Security):
        if "Assignment" in order_event.message:
            if order_event.fill_price != 3400.0:
                raise AssertionError("Option was not assigned at expected strike price (3400)")

            if order_event.direction != OrderDirection.BUY or future.holdings.quantity != 1:
                raise AssertionError(f"Expected Qty: 1 futures holdings for assigned future {future.symbol}, found {future.holdings.quantity}")

            return

        if order_event.direction == OrderDirection.SELL and future.holdings.quantity != 0:
            # We buy back the underlying at expiration, so we expect a neutral position then
            raise AssertionError(f"Expected no holdings when liquidating future contract {future.symbol}")

    def assert_future_option_contract_order(self, order_event: OrderEvent, option: Security):
        if order_event.direction == OrderDirection.SELL and option.holdings.quantity != -1:
            raise AssertionError(f"No holdings were created for option contract {option.symbol}")

        if order_event.is_assignment and option.holdings.quantity != 0:
            raise AssertionError(f"Holdings were found after option contract was assigned: {option.symbol}")

    def on_end_of_algorithm(self):
        if self.portfolio.invested:
            raise AssertionError(f"Expected no holdings at end of algorithm, but are invested in: {', '.join([str(i.id) for i in self.portfolio.keys()])}")
