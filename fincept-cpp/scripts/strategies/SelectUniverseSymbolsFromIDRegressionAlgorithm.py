# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-BCEB1931
# Category: Universe Selection
# Description: Regression algorithm asserting that universe symbols selection can be done by returning the symbol IDs in the selecti...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting that universe symbols selection can be done by returning the symbol IDs in the selection function
### </summary>
class SelectUniverseSymbolsFromIDRegressionAlgorithm(QCAlgorithm):
    '''
    Regression algorithm asserting that universe symbols selection can be done by returning the symbol IDs in the selection function
    '''

    def initialize(self):
        self.set_start_date(2014, 3, 24)
        self.set_end_date(2014, 3, 26)
        self.set_cash(100000)

        self._securities = []
        self.universe_settings.resolution = Resolution.DAILY
        self.add_universe(self.select_symbol)

    def select_symbol(self, fundamental):
        symbols = [x.symbol for x in fundamental]

        if not symbols:
            return []

        self.log(f"Symbols: {', '.join([str(s) for s in symbols])}")
        # Just for testing, but more filtering could be done here as shown below:
        #symbols = [x.symbol for x in fundamental if x.asset_classification.morningstar_sector_code == MorningstarSectorCode.TECHNOLOGY]

        history = self.history(symbols, datetime(1998, 1, 1), self.time, Resolution.DAILY)

        all_time_highs = history['high'].unstack(0).max()
        last_closes = history['close'].unstack(0).iloc[-1]
        security_ids = (last_closes / all_time_highs).sort_values().index[-5:]

        return security_ids

    def on_securities_changed(self, changes):
        self._securities.extend(changes.added_securities)

    def on_end_of_algorithm(self):
        if not self._securities:
            raise Exception("No securities were selected")
