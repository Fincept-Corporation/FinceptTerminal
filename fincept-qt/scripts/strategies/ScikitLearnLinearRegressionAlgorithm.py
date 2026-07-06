# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1244BB8B
# Category: Regression Test
# Description: Linear regression momentum strategy. Uses a rolling 30-period
#   price window to compute linear regression slope. Buys when slope is
#   positive (uptrend), sells when slope turns negative.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class ScikitLearnLinearRegressionAlgorithm(QCAlgorithm):
    """Linear regression slope momentum strategy."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._lookback = 30
        self._prices = []

    def on_data(self, data):
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        self._prices.append(price)
        if len(self._prices) > self._lookback:
            self._prices = self._prices[-self._lookback:]

        if len(self._prices) < self._lookback:
            return

        # Simple linear regression slope
        n = len(self._prices)
        x_mean = (n - 1) / 2.0
        y_mean = sum(self._prices) / n
        numerator = sum((i - x_mean) * (p - y_mean) for i, p in enumerate(self._prices))
        denominator = sum((i - x_mean) ** 2 for i in range(n))
        slope = numerator / denominator if denominator != 0 else 0

        # Normalize slope by price level
        norm_slope = slope / y_mean if y_mean != 0 else 0

        if not self.portfolio.invested and norm_slope > 0.001:
            self.set_holdings(self.symbol, 1)
        elif self.portfolio.invested and norm_slope < -0.001:
            self.liquidate()
