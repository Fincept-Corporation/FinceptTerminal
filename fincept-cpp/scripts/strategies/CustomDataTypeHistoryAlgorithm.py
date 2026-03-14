# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1A4DBA47
# Category: Custom Data
# Description: Gap and momentum strategy inspired by custom data patterns.
#   Buys when today's open is above yesterday's close (gap up) with positive
#   10-day momentum. Sells when gap down occurs or momentum turns negative.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class CustomDataTypeHistoryAlgorithm(QCAlgorithm):
    """Gap-up momentum strategy."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._momp = self.momp(self.symbol, 10, Resolution.DAILY)
        self._prev_close = 0

    def on_data(self, data):
        if not self._momp.is_ready:
            return
        if self.symbol not in data:
            return

        bar = data[self.symbol]
        mom_val = self._momp.current.value

        if self._prev_close > 0:
            gap_pct = (bar.open - self._prev_close) / self._prev_close * 100

            if not self.portfolio.invested:
                # Gap up + positive momentum
                if gap_pct > 0.1 and mom_val > 0:
                    self.set_holdings(self.symbol, 1)
            else:
                # Gap down or negative momentum
                if gap_pct < -0.3 or mom_val < -1:
                    self.liquidate()

        self._prev_close = bar.close
