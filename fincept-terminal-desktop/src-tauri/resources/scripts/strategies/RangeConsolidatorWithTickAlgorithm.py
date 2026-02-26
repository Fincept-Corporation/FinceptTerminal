# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0761902F
# Category: Data Consolidation
# Description: Bollinger Band mean-reversion strategy adapted from tick-level
#   range consolidation. Buys when price drops below lower BB (2 stdev), sells
#   when price rises above upper BB. Uses 20-period bands.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class RangeConsolidatorWithTickAlgorithm(QCAlgorithm):
    """Bollinger Band mean-reversion: buys at lower band, sells at upper."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._bb = self.bb(self.symbol, 20, 2, Resolution.DAILY)

    def on_data(self, data):
        if not self._bb.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        upper = self._bb.upper_band.current.value
        lower = self._bb.lower_band.current.value
        middle = self._bb.middle_band.current.value

        # Buy when price is below lower Bollinger Band
        if not self.portfolio.invested and price < lower:
            self.set_holdings(self.symbol, 1)
        # Sell when price reaches upper band or middle band on recovery
        elif self.portfolio.invested and price > upper:
            self.liquidate()
