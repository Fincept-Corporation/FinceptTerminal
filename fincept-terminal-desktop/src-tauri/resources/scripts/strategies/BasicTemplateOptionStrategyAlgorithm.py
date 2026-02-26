# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-185C6BF9
# Category: Options
# Description: Options-inspired hedged momentum strategy. Buys SPY when above
#   both 10 and 30-day EMAs (strong uptrend). Uses tight stop (2%) to simulate
#   option-like risk profile. Re-enters on trend resumption.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicTemplateOptionStrategyAlgorithm(QCAlgorithm):
    """Hedged momentum with tight stop-loss."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._ema10 = self.ema(self.symbol, 10, Resolution.DAILY)
        self._ema30 = self.ema(self.symbol, 30, Resolution.DAILY)
        self._entry_price = 0

    def on_data(self, data):
        if not self._ema30.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        e10 = self._ema10.current.value
        e30 = self._ema30.current.value

        if not self.portfolio.invested:
            if price > e10 and e10 > e30:
                self.set_holdings(self.symbol, 1)
                self._entry_price = price
        else:
            # Tight stop-loss (2%) or trend reversal
            if price < self._entry_price * 0.98 or e10 < e30:
                self.liquidate()
