# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1D2112EA
# Category: Options
# Description: Regression algorithm to test the OptionChainedUniverseSelectionModel class
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm to test the OptionChainedUniverseSelectionModel class
### </summary>
class OptionChainedUniverseSelectionModelRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.universe_settings.resolution = Resolution.MINUTE
        self.set_start_date(2014, 6, 6)
        self.set_end_date(2014, 6, 6)
        self.set_cash(100000)

        universe = self.add_universe("my-minute-universe-name", lambda time: [ "AAPL", "TWX" ])
        self.add_universe_selection(
            OptionChainedUniverseSelectionModel(
                universe,
                lambda u: (u.strikes(-2, +2)
                                     # Expiration method accepts TimeSpan objects or integer for days.
                                     # The following statements yield the same filtering criteria
                                     .expiration(0, 180))
            )
        )

    def on_data(self, slice):
        if self.portfolio.invested or not (self.is_market_open("AAPL") and self.is_market_open("TWX")): return
        values = list(map(lambda x: x.value, filter(lambda x: x.key == "?AAPL" or x.key == "?TWX",  slice.option_chains)))
        for chain in values:
            # we sort the contracts to find at the money (ATM) contract with farthest expiration
            contracts = sorted(sorted(sorted(chain, \
                key = lambda x: abs(chain.underlying.price - x.strike)), \
                key = lambda x: x.expiry, reverse=True), \
                key = lambda x: x.right, reverse=True)

            # if found, trade it
            if len(contracts) == 0: return
            symbol = contracts[0].symbol
            self.market_order(symbol, 1)
            self.market_on_close_order(symbol, -1)
