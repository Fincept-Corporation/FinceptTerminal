# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0AD9A342
# Category: General Strategy
# Description: Protective position management strategy inspired by covered call
#   mechanics. Buys SPY and uses trailing stop logic to protect profits. Enters
#   on EMA uptrend, exits with 3% trailing stop loss.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class CoveredAndProtectiveCallStrategiesAlgorithm(QCAlgorithm):
    """Trend-following with trailing stop loss (protective strategy)."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._ema = self.ema(self.symbol, 20, Resolution.DAILY)
        self._highest_since_entry = 0.0

    def on_data(self, data):
        if not self._ema.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close

        if not self.portfolio.invested:
            # Enter when price above EMA (uptrend)
            if price > self._ema.current.value:
                self.set_holdings(self.symbol, 1)
                self._highest_since_entry = price
        else:
            # Track highest price since entry
            if price > self._highest_since_entry:
                self._highest_since_entry = price

            # Trailing stop: exit if price drops 3% from peak
            drawdown = (self._highest_since_entry - price) / self._highest_since_entry
            if drawdown > 0.03:
                self.liquidate()
                self._highest_since_entry = 0.0
