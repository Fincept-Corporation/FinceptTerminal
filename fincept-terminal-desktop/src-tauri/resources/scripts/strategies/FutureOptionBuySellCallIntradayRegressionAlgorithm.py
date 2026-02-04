# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1855F876
# Category: Options
# Description: This regression algorithm tests In The Money (ITM) future option calls across different strike prices. We expect 6 or...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests In The Money (ITM) future option calls across different strike prices.
### We expect 6 orders from the algorithm, which are:
###
###   * (1) Initial entry, buy ES Call Option (ES19M20 expiring ITM)
###   * (2) Initial entry, sell ES Call Option at different strike (ES20H20 expiring ITM)
###   * [2] Option assignment, opens a position in the underlying (ES20H20, Qty: -1)
###   * [2] Future contract liquidation, due to impending expiry
###   * [1] Option exercise, receive 1 ES19M20 future contract
###   * [1] Liquidate ES19M20 contract, due to expiry
###
### Additionally, we test delistings for future options and assert that our
### portfolio holdings reflect the orders the algorithm has submitted.
### </summary>
class FutureOptionBuySellCallIntradayRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 1, 5)
        self.set_end_date(2020, 6, 30)

        self.es20h20 = self.add_future_contract(
            Symbol.create_future(
                Futures.Indices.SP_500_E_MINI,
                Market.CME,
                datetime(2020, 3, 20)
            ),
            Resolution.MINUTE).symbol

        self.es19m20 = self.add_future_contract(
            Symbol.create_future(
                Futures.Indices.SP_500_E_MINI,
                Market.CME,
                datetime(2020, 6, 19)
            ),
            Resolution.MINUTE).symbol

        # Select a future option expiring ITM, and adds it to the algorithm.
        self.es_options = [
            self.add_future_option_contract(i, Resolution.MINUTE).symbol
            for i in (list(self.option_chain(self.es19m20)) + list(self.option_chain(self.es20h20)))
            if i.id.strike_price == 3200.0 and i.id.option_right == OptionRight.CALL
        ]

        self.expected_contracts = [
            Symbol.create_option(self.es20h20, Market.CME, OptionStyle.AMERICAN, OptionRight.CALL, 3200.0, datetime(2020, 3, 20)),
            Symbol.create_option(self.es19m20, Market.CME, OptionStyle.AMERICAN, OptionRight.CALL, 3200.0, datetime(2020, 6, 19))
        ]

        for es_option in self.es_options:
            if es_option not in self.expected_contracts:
                raise AssertionError(f"Contract {es_option} was not found in the chain")

        self.schedule.on(self.date_rules.tomorrow, self.time_rules.after_market_open(self.es19m20, 1), self.schedule_callback_buy)

    def schedule_callback_buy(self):
        self.market_order(self.es_options[0], 1)
        self.market_order(self.es_options[1], -1)

    def on_end_of_algorithm(self):
        if self.portfolio.invested:
            raise AssertionError(f"Expected no holdings at end of algorithm, but are invested in: {', '.join([str(i.id) for i in self.portfolio.keys()])}")
