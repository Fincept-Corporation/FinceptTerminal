# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0A8C8781
# Category: Universe Selection
# Description: Sector rotation strategy inspired by fundamental universe selection.
#   Rotates between SPY and AAPL based on relative momentum. Holds the stronger
#   performer over the past 20 days. Rebalances weekly.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class FundamentalUniverseSelectionAlgorithm(QCAlgorithm):
    """Relative momentum rotation: holds the stronger of SPY vs AAPL."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.sym_a = "SPY"
        self.sym_b = "AAPL"

        self.add_equity(self.sym_a, Resolution.DAILY)
        self.add_equity(self.sym_b, Resolution.DAILY)

        self._sma_a = self.sma(self.sym_a, 20, Resolution.DAILY)
        self._sma_b = self.sma(self.sym_b, 20, Resolution.DAILY)

        self._last_trade_week = -1

    def on_data(self, data):
        if not self._sma_a.is_ready or not self._sma_b.is_ready:
            return

        # Rebalance weekly
        current_week = self.time.isocalendar()[1]
        if current_week == self._last_trade_week:
            return
        self._last_trade_week = current_week

        if self.sym_a not in data or self.sym_b not in data:
            return

        # Relative strength: price / SMA ratio
        rs_a = data[self.sym_a].close / self._sma_a.current.value if self._sma_a.current.value > 0 else 0
        rs_b = data[self.sym_b].close / self._sma_b.current.value if self._sma_b.current.value > 0 else 0

        # Hold the one with stronger momentum
        if rs_a > rs_b:
            if self.portfolio[self.sym_b].invested:
                self.liquidate(self.sym_b)
            self.set_holdings(self.sym_a, 0.95)
        else:
            if self.portfolio[self.sym_a].invested:
                self.liquidate(self.sym_a)
            self.set_holdings(self.sym_b, 0.95)
