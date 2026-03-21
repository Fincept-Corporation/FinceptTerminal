# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-198E83AF
# Category: Regression Test
# Description: Volume-weighted trend strategy. Tracks volume relative to its
#   20-day average. Buys when price is rising and volume is above average
#   (confirming trend). Sells on volume contraction below average.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class SliceGetByTypeRegressionAlgorithm(QCAlgorithm):
    """Volume-confirmed trend following strategy."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._ema = self.ema(self.symbol, 20, Resolution.DAILY)
        self._volumes = []

    def on_data(self, data):
        if not self._ema.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        volume = data[self.symbol].volume

        self._volumes.append(volume)
        if len(self._volumes) > 20:
            self._volumes = self._volumes[-20:]

        if len(self._volumes) < 20:
            return

        avg_vol = sum(self._volumes) / len(self._volumes)
        ema_val = self._ema.current.value

        if not self.portfolio.invested:
            if price > ema_val and volume > avg_vol * 1.1:
                self.set_holdings(self.symbol, 1)
        else:
            if price < ema_val:
                self.liquidate()
