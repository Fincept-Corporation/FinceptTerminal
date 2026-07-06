# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-15D6B08D
# Category: Alpha Model
# Description: Multi-signal alpha strategy combining RSI and EMA crossover.
#   Generates buy signal when RSI < 40 AND fast EMA > slow EMA. Sells when
#   RSI > 70 OR fast EMA crosses below slow EMA.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class InsightTagAlphaRegressionAlgorithm(QCAlgorithm):
    """Combined RSI + EMA crossover alpha strategy."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._rsi = self.rsi(self.symbol, 14, Resolution.DAILY)
        self._fast_ema = self.ema(self.symbol, 10, Resolution.DAILY)
        self._slow_ema = self.ema(self.symbol, 30, Resolution.DAILY)

    def on_data(self, data):
        if not self._rsi.is_ready or not self._slow_ema.is_ready:
            return
        if self.symbol not in data:
            return

        rsi_val = self._rsi.current.value
        fast = self._fast_ema.current.value
        slow = self._slow_ema.current.value

        if not self.portfolio.invested:
            if rsi_val < 40 and fast > slow:
                self.set_holdings(self.symbol, 1)
        else:
            if rsi_val > 70 or fast < slow:
                self.liquidate()
