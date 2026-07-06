# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-129041A8
# Category: Options
# Description: Short-term mean reversion strategy inspired by options expiry
#   patterns. Buys on RSI oversold (<35) conditions and sells on overbought
#   (>65) or after 5-day maximum holding period.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class FutureOptionShortCallITMExpiryRegressionAlgorithm(QCAlgorithm):
    """Short-term RSI mean-reversion with time-based exit."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._rsi = self.rsi(self.symbol, 14, Resolution.DAILY)
        self._entry_bar = 0
        self._bar_count = 0
        self._max_hold = 5

    def on_data(self, data):
        if not self._rsi.is_ready:
            return
        if self.symbol not in data:
            return

        self._bar_count += 1
        rsi_val = self._rsi.current.value

        if not self.portfolio.invested:
            if rsi_val < 35:
                self.set_holdings(self.symbol, 1)
                self._entry_bar = self._bar_count
        else:
            bars_held = self._bar_count - self._entry_bar
            if rsi_val > 65 or bars_held >= self._max_hold:
                self.liquidate()
