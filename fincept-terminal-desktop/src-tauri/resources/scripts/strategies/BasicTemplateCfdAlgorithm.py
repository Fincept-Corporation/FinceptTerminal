# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-07CF5BDC
# Category: Template
# Description: Momentum strategy adapted from CFD trading template. Uses dual
#   SMA (5/20) to detect short-term momentum shifts. Buys when 5-day SMA crosses
#   above 20-day SMA, exits on crossunder.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicTemplateCfdAlgorithm(QCAlgorithm):
    """Dual SMA momentum strategy (adapted from CFD template)."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._fast = self.sma(self.symbol, 5, Resolution.DAILY)
        self._slow = self.sma(self.symbol, 20, Resolution.DAILY)

    def on_data(self, data):
        if not self._fast.is_ready or not self._slow.is_ready:
            return
        if self.symbol not in data:
            return

        fast_val = self._fast.current.value
        slow_val = self._slow.current.value

        if not self.portfolio.invested and fast_val > slow_val:
            self.set_holdings(self.symbol, 1)
        elif self.portfolio.invested and fast_val < slow_val:
            self.liquidate()
