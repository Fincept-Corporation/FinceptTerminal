# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-DB3B5321
# Category: Options
# Description: This example demonstrates how to add and trade SPX index weekly options
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This example demonstrates how to add and trade SPX index weekly options
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="options" />
### <meta name="tag" content="indexes" />
class BasicTemplateSPXWeeklyIndexOptionsAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 1, 10)
        self.set_cash(1000000)

        # regular option SPX contracts
        self.spx_options = self.add_index_option("SPX")
        self.spx_options.set_filter(lambda u: (u.strikes(0, 1).expiration(0, 30)))

        # weekly option SPX contracts
        spxw = self.add_index_option("SPX", "SPXW")
        # set our strike/expiry filter for this option chain
        spxw.set_filter(lambda u: (u.strikes(0, 1)
                                     # single week ahead since there are many SPXW contracts and we want to preserve performance
                                     .expiration(0, 7)
                                     .include_weeklys()))

        self.spxw_option = spxw.symbol

    def on_data(self,slice):
        if self.portfolio.invested: return

        chain = slice.option_chains.get_value(self.spxw_option)
        if chain is None:
            return

        # we sort the contracts to find at the money (ATM) contract with closest expiration
        contracts = sorted(sorted(sorted(chain, \
            key = lambda x: x.expiry), \
            key = lambda x: abs(chain.underlying.price - x.strike)), \
            key = lambda x: x.right, reverse=True)

        # if found, buy until it expires
        if len(contracts) == 0: return
        symbol = contracts[0].symbol
        self.market_order(symbol, 1)

    def on_order_event(self, order_event):
        self.debug(str(order_event))
