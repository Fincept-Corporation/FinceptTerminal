# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-16536530
# Category: Options
# Description: Daily momentum strategy inspired by futures options daily patterns.
#   Uses MACD histogram as primary signal. Buys on positive histogram crossover,
#   sells on negative crossover. Simple daily resolution trend capture.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class FutureOptionDailyRegressionAlgorithm(QCAlgorithm):
    """MACD histogram crossover strategy."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._macd = self.macd(self.symbol, 12, 26, 9, Resolution.DAILY)
        self._prev_histogram = 0

    def on_data(self, data):
        if not self._macd.is_ready:
            return
        if self.symbol not in data:
            return

        histogram = self._macd.current.value - self._macd.signal.current.value
        crossed_up = self._prev_histogram <= 0 and histogram > 0
        crossed_down = self._prev_histogram >= 0 and histogram < 0
        self._prev_histogram = histogram

        if not self.portfolio.invested and crossed_up:
            self.set_holdings(self.symbol, 1)
        elif self.portfolio.invested and crossed_down:
            self.liquidate()
