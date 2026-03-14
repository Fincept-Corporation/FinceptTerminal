# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-68DA46AE
# Category: Options
# Description: This regression algorithm tests Out of The Money (OTM) future option expiry for short calls. We expect 2 orders from ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests Out of The Money (OTM) future option expiry for short calls.
### We expect 2 orders from the algorithm, which are:
###
###   * Initial entry, sell ES Call Option (expiring OTM)
###     - Profit the option premium, since the option was not assigned.
###
###   * Liquidation of ES call OTM contract on the last trade date
###
### Additionally, we test delistings for future options and assert that our
### portfolio holdings reflect the orders the algorithm has submitted.
### </summary>
class FutureOptionShortCallOTMExpiryRegressionAlgorithm(QCAlgorithm):

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
                    [x for x in self.option_chain(self.es19m20) if x.id.strike_price >= 3400.0 and x.id.option_right == OptionRight.CALL],
                    key=lambda x: x.id.strike_price
                )
            )[0], Resolution.MINUTE).symbol

        self.expected_contract = Symbol.create_option(self.es19m20, Market.CME, OptionStyle.AMERICAN, OptionRight.CALL, 3400.0, datetime(2020, 6, 19))
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
            raise AssertionError(f"Expected no order events for underlying Symbol {security.symbol}")

        if security.symbol == self.expected_contract:
            self.assert_future_option_contract_order(order_event, security)

        else:
            raise AssertionError(f"Received order event for unknown Symbol: {order_event.symbol}")

        self.log(f"{order_event}")

    def assert_future_option_contract_order(self, order_event: OrderEvent, option_contract: Security):
        if order_event.direction == OrderDirection.SELL and option_contract.holdings.quantity != -1:
            raise AssertionError(f"No holdings were created for option contract {option_contract.symbol}")

        if order_event.direction == OrderDirection.BUY and option_contract.holdings.quantity != 0:
            raise AssertionError("Expected no options holdings after closing position")

        if order_event.is_assignment:
            raise AssertionError(f"Assignment was not expected for {order_event.symbol}")

    def on_end_of_algorithm(self):
        if self.portfolio.invested:
            raise AssertionError(f"Expected no holdings at end of algorithm, but are invested in: {', '.join([str(i.id) for i in self.portfolio.keys()])}")
