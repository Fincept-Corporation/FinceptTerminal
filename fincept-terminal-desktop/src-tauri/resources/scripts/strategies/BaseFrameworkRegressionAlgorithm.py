# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0A7F740C
# Category: Regression Test
# Description: Multi-stock equal-weight alpha strategy. Allocates equal weight
#   across AAPL, MSFT, SPY, and GOOGL when any is above its 20-day EMA.
#   Rebalances daily. Originally a framework regression test for alpha models.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BaseFrameworkRegressionAlgorithm(QCAlgorithm):
    """Equal-weight multi-stock strategy with EMA trend filter."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbols = ["AAPL", "MSFT", "SPY", "GOOGL"]
        self._emas = {}

        for sym in self.symbols:
            self.add_equity(sym, Resolution.DAILY)
            self._emas[sym] = self.ema(sym, 20, Resolution.DAILY)

        self._last_rebalance_month = -1

    def on_data(self, data):
        # Rebalance monthly
        if self.time.month == self._last_rebalance_month:
            return
        self._last_rebalance_month = self.time.month

        # Check which symbols are above their EMA (uptrend)
        longs = []
        for sym in self.symbols:
            if sym not in data:
                continue
            ema = self._emas[sym]
            if not ema.is_ready:
                continue
            if data[sym].close > ema.current.value:
                longs.append(sym)

        # Liquidate symbols not in longs
        for sym in self.symbols:
            if sym not in longs and self.portfolio[sym].invested:
                self.liquidate(sym)

        # Equal-weight allocation
        if longs:
            weight = 0.95 / len(longs)
            for sym in longs:
                self.set_holdings(sym, weight)
