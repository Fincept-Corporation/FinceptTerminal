# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-17047AD3
# Category: Regression Test
# Description: Historical price channel breakout strategy. Tracks 15-day
#   high and low channels. Buys on upper channel breakout, sells on lower
#   channel breakdown. Classic Donchian channel approach.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class HistoryAuxiliaryDataRegressionAlgorithm(QCAlgorithm):
    """Donchian channel breakout strategy."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._max = self.max(self.symbol, 15, Resolution.DAILY)
        self._min = self.min(self.symbol, 15, Resolution.DAILY)
        self._bar_count = 0

    def on_data(self, data):
        if not self._max.is_ready or not self._min.is_ready:
            return
        if self.symbol not in data:
            return

        self._bar_count += 1
        if self._bar_count <= 15:
            return

        price = data[self.symbol].close
        upper = self._max.current.value
        lower = self._min.current.value

        if not self.portfolio.invested:
            if price >= upper:
                self.set_holdings(self.symbol, 1)
        else:
            if price <= lower:
                self.liquidate()
