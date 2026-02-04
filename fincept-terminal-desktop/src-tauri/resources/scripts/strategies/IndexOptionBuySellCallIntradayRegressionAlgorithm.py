# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C204FEB8
# Category: Options
# Description: This regression algorithm tests In The Money (ITM) index option calls across different strike prices. We expect 4* or...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests In The Money (ITM) index option calls across different strike prices.
### We expect 4* orders from the algorithm, which are:
###
###   * (1) Initial entry, buy SPX Call Option (SPXF21 expiring ITM)
###   * (2) Initial entry, sell SPX Call Option at different strike (SPXF21 expiring ITM)
###   * [2] Option assignment, settle into cash
###   * [1] Option exercise, settle into cash
###
### Additionally, we test delistings for index options and assert that our
### portfolio holdings reflect the orders the algorithm has submitted.
###
### * Assignments are counted as orders
### </summary>
class IndexOptionBuySellCallIntradayRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 1, 31)

        spx = self.add_index("SPX", Resolution.MINUTE).symbol

        # Select a index option expiring ITM, and adds it to the algorithm.
        spx_options = list(sorted([
            self.add_index_option_contract(i, Resolution.MINUTE).symbol \
                for i in self.option_chain(spx)\
                    if (i.id.strike_price == 3700 or i.id.strike_price == 3800) and i.id.option_right == OptionRight.CALL and i.id.date.year == 2021 and i.id.date.month == 1],
            key=lambda x: x.id.strike_price
        ))

        expectedContract3700 = Symbol.create_option(
            spx,
            Market.USA,
            OptionStyle.EUROPEAN,
            OptionRight.CALL,
            3700,
            datetime(2021, 1, 15)
        )

        expectedContract3800 = Symbol.create_option(
            spx,
            Market.USA,
            OptionStyle.EUROPEAN,
            OptionRight.CALL,
            3800,
            datetime(2021, 1, 15)
        )

        if len(spx_options) != 2:
            raise Exception(f"Expected 2 index options symbols from chain provider, found {spx_options.count}")

        if spx_options[0] != expectedContract3700:
            raise Exception(f"Contract {expectedContract3700} was not found in the chain, found instead: {spx_options[0]}")

        if spx_options[1] != expectedContract3800:
            raise Exception(f"Contract {expectedContract3800} was not found in the chain, found instead: {spx_options[1]}")

        self.schedule.on(self.date_rules.tomorrow, self.time_rules.after_market_open(spx, 1), lambda: self.after_market_open_trade(spx_options))
        self.schedule.on(self.date_rules.tomorrow, self.time_rules.noon, lambda: self.liquidate())

    def after_market_open_trade(self, spx_options):
        self.market_order(spx_options[0], 1)
        self.market_order(spx_options[1], -1)

    ### <summary>
    ### Ran at the end of the algorithm to ensure the algorithm has no holdings
    ### </summary>
    ### <exception cref="Exception">The algorithm has holdings</exception>
    def on_end_of_algorithm(self):
        if self.portfolio.invested:
            raise Exception(f"Expected no holdings at end of algorithm, but are invested in: {', '.join(self.portfolio.keys())}")
