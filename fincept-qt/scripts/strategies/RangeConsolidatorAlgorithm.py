# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-052CED9D
# Category: Data Consolidation
# Description: Breakout strategy using price range analysis. Buys when the
#   current close breaks above the 10-day high, sells when it drops below the
#   10-day low. Inspired by range consolidation breakout patterns.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class RangeConsolidatorAlgorithm(QCAlgorithm):
    """Price range breakout strategy: enters on new highs, exits on new lows."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._max = self.max(self.symbol, 10, Resolution.DAILY)
        self._min = self.min(self.symbol, 10, Resolution.DAILY)

    def on_data(self, data):
        if not self._max.is_ready or not self._min.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        upper = self._max.current.value
        lower = self._min.current.value

        # Breakout above 10-day high → buy
        if not self.portfolio.invested and price >= upper:
            self.set_holdings(self.symbol, 1)
        # Breakdown below 10-day low → sell
        elif self.portfolio.invested and price <= lower:
            self.liquidate()
