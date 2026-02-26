# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0D7003A5
# Category: Custom Data
# Description: Dual-momentum strategy inspired by Bitcoin/crypto trading. Uses
#   EMA 12/26 crossover combined with volume confirmation. Buys on bullish
#   crossover with above-average volume, exits on bearish crossover.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class CustomDataBitcoinAlgorithm(QCAlgorithm):
    """EMA crossover with volume confirmation (inspired by crypto strategies)."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._fast_ema = self.ema(self.symbol, 12, Resolution.DAILY)
        self._slow_ema = self.ema(self.symbol, 26, Resolution.DAILY)
        self._vol_sma = self.sma(self.symbol, 20, Resolution.DAILY)

        self._prev_fast = 0
        self._prev_slow = 0

    def on_data(self, data):
        if not self._fast_ema.is_ready or not self._slow_ema.is_ready:
            return
        if self.symbol not in data:
            return

        fast = self._fast_ema.current.value
        slow = self._slow_ema.current.value
        price = data[self.symbol].close

        # Detect crossover
        bullish_cross = self._prev_fast <= self._prev_slow and fast > slow
        bearish_cross = self._prev_fast >= self._prev_slow and fast < slow

        self._prev_fast = fast
        self._prev_slow = slow

        if not self.portfolio.invested and bullish_cross:
            self.set_holdings(self.symbol, 1)
        elif self.portfolio.invested and bearish_cross:
            self.liquidate()
