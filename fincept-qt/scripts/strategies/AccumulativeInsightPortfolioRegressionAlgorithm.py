# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-19E8A564
# Category: Portfolio Management
# Description: Accumulative position building strategy. Starts with 25%
#   allocation on initial EMA crossover. Adds 25% on each pullback to EMA
#   that bounces. Maximum 100% allocation. Exits on bearish EMA cross.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class AccumulativeInsightPortfolioRegressionAlgorithm(QCAlgorithm):
    """Accumulative position building with EMA trend."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._fast_ema = self.ema(self.symbol, 10, Resolution.DAILY)
        self._slow_ema = self.ema(self.symbol, 30, Resolution.DAILY)
        self._current_weight = 0
        self._touched_ema = False

    def on_data(self, data):
        if not self._slow_ema.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        fast = self._fast_ema.current.value
        slow = self._slow_ema.current.value

        if fast > slow:
            # Uptrend
            if self._current_weight == 0:
                self._current_weight = 0.5
                self.set_holdings(self.symbol, self._current_weight)
                self._touched_ema = False
            elif price <= fast * 1.002:
                self._touched_ema = True
            elif self._touched_ema and price > fast and self._current_weight < 1.0:
                self._current_weight = min(1.0, self._current_weight + 0.25)
                self.set_holdings(self.symbol, self._current_weight)
                self._touched_ema = False
        else:
            # Downtrend - exit all
            if self._current_weight > 0:
                self.liquidate()
                self._current_weight = 0
                self._touched_ema = False
