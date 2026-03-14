# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-161A4DE8
# Category: Universe Selection
# Description: Weekly rotation strategy inspired by universe selection. Buys
#   at the start of each week if price is above 10-day SMA, exits at the end
#   of the week. Captures weekly momentum trends.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class WeeklyUniverseSelectionRegressionAlgorithm(QCAlgorithm):
    """Weekly momentum rotation with SMA filter."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._sma = self.sma(self.symbol, 10, Resolution.DAILY)
        self._last_week = -1

    def on_data(self, data):
        if not self._sma.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        current_week = self.time.isocalendar()[1]

        if current_week != self._last_week:
            self._last_week = current_week
            # Weekly decision: enter if price > SMA, exit otherwise
            if price > self._sma.current.value:
                if not self.portfolio.invested:
                    self.set_holdings(self.symbol, 1)
            else:
                if self.portfolio.invested:
                    self.liquidate()
