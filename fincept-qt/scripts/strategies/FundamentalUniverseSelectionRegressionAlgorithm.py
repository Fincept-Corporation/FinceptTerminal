# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-19976618
# Category: Universe Selection
# Description: Dual momentum strategy with monthly rebalancing. Compares
#   20-day rate of change between SPY and trend. Goes long when momentum
#   is positive, flat otherwise. Monthly rebalance frequency.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class FundamentalUniverseSelectionRegressionAlgorithm(QCAlgorithm):
    """Monthly rebalancing momentum strategy."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._momp = self.momp(self.symbol, 20, Resolution.DAILY)
        self._last_month = -1

    def on_data(self, data):
        if not self._momp.is_ready:
            return
        if self.symbol not in data:
            return

        current_month = self.time.month
        if current_month == self._last_month:
            return
        self._last_month = current_month

        mom_val = self._momp.current.value

        if mom_val > 0:
            if not self.portfolio.invested:
                self.set_holdings(self.symbol, 1)
        else:
            if self.portfolio.invested:
                self.liquidate()
