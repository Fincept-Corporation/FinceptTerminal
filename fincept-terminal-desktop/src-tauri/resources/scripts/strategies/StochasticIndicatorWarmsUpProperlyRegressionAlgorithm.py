# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1A300907
# Category: Indicators
# Description: Stochastic oscillator strategy with SMA trend filter. Buys when
#   stochastic %K crosses above 20 (oversold) while price is above 50-day SMA.
#   Sells when %K crosses below 80 (overbought) or price drops below SMA.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class StochasticIndicatorWarmsUpProperlyRegressionAlgorithm(QCAlgorithm):
    """Stochastic oversold/overbought strategy with SMA filter."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._sma = self.sma(self.symbol, 50, Resolution.DAILY)
        # Manual stochastic: track 14-day high/low
        self._highs = []
        self._lows = []
        self._prev_k = 50

    def on_data(self, data):
        if not self._sma.is_ready:
            return
        if self.symbol not in data:
            return

        bar = data[self.symbol]
        price = bar.close
        self._highs.append(bar.high)
        self._lows.append(bar.low)
        if len(self._highs) > 14:
            self._highs = self._highs[-14:]
            self._lows = self._lows[-14:]

        if len(self._highs) < 14:
            return

        highest = max(self._highs)
        lowest = min(self._lows)
        k_range = highest - lowest
        k = ((price - lowest) / k_range * 100) if k_range > 0 else 50

        sma_val = self._sma.current.value
        crossed_up_20 = self._prev_k <= 20 and k > 20
        crossed_down_80 = self._prev_k >= 80 and k < 80
        self._prev_k = k

        if not self.portfolio.invested:
            if crossed_up_20 and price > sma_val:
                self.set_holdings(self.symbol, 1)
        else:
            if crossed_down_80 or price < sma_val:
                self.liquidate()
