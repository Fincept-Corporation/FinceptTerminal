# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0D15187F
# Category: Indicators
# Description: Triple SMA crossover strategy demonstrating indicator registration.
#   Uses 5/10/20-period SMAs with registered auto-update. Buys when all three
#   are aligned bullish (5 > 10 > 20), sells when alignment breaks.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class RegisterIndicatorRegressionAlgorithm(QCAlgorithm):
    """Triple SMA crossover with indicator registration."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        # Create and register SMAs (name, period)
        self._sma5 = SimpleMovingAverage("SMA5", 5)
        self._sma10 = SimpleMovingAverage("SMA10", 10)
        self._sma20 = SimpleMovingAverage("SMA20", 20)

        self.register_indicator(self.symbol, self._sma5, Resolution.DAILY)
        self.register_indicator(self.symbol, self._sma10, Resolution.DAILY)
        self.register_indicator(self.symbol, self._sma20, Resolution.DAILY)

    def on_data(self, data):
        if not self._sma20.is_ready:
            return
        if self.symbol not in data:
            return

        s5 = self._sma5.current.value
        s10 = self._sma10.current.value
        s20 = self._sma20.current.value

        # All three aligned bullish: 5 > 10 > 20
        if not self.portfolio.invested and s5 > s10 > s20:
            self.set_holdings(self.symbol, 1)
        # Alignment broken
        elif self.portfolio.invested and s5 < s10:
            self.liquidate()
