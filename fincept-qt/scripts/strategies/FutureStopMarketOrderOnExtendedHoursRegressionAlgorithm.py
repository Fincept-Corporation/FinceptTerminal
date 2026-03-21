# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-119E482D
# Category: Futures
# Description: Trailing stop strategy inspired by futures stop-market orders.
#   Enters long when price crosses above 20-day SMA, exits with a 3% trailing
#   stop loss. Re-enters on next SMA crossover.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class FutureStopMarketOrderOnExtendedHoursRegressionAlgorithm(QCAlgorithm):
    """Trailing stop strategy using SMA entry and percentage-based trailing stop."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._sma = self.sma(self.symbol, 20, Resolution.DAILY)
        self._highest_since_entry = 0
        self._stop_pct = 0.03  # 3% trailing stop

    def on_data(self, data):
        if not self._sma.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        sma_val = self._sma.current.value

        if not self.portfolio.invested:
            if price > sma_val:
                self.set_holdings(self.symbol, 1)
                self._highest_since_entry = price
        else:
            if price > self._highest_since_entry:
                self._highest_since_entry = price
            stop_level = self._highest_since_entry * (1 - self._stop_pct)
            if price < stop_level:
                self.liquidate()
