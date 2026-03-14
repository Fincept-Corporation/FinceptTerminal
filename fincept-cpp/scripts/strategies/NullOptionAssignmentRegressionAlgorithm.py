# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-16C48497
# Category: Options
# Description: Volatility-adjusted position sizing strategy. Buys when ATR is
#   below its 20-day average (low volatility) with full position. Reduces to
#   half position when ATR rises above average. Exits on SMA crossdown.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class NullOptionAssignmentRegressionAlgorithm(QCAlgorithm):
    """Volatility-adjusted position sizing with SMA trend filter."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._sma = self.sma(self.symbol, 20, Resolution.DAILY)
        self._atr = self.atr(self.symbol, 14, Resolution.DAILY)
        self._atr_sma_values = []

    def on_data(self, data):
        if not self._sma.is_ready or not self._atr.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        sma_val = self._sma.current.value
        atr_val = self._atr.current.value

        # Track ATR average
        self._atr_sma_values.append(atr_val)
        if len(self._atr_sma_values) > 20:
            self._atr_sma_values = self._atr_sma_values[-20:]
        avg_atr = sum(self._atr_sma_values) / len(self._atr_sma_values)

        if price > sma_val:
            target = 1.0 if atr_val < avg_atr else 0.5
            if not self.portfolio.invested:
                self.set_holdings(self.symbol, target)
        else:
            if self.portfolio.invested:
                self.liquidate()
