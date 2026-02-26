# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-181E0A50
# Category: General Strategy
# Description: Bubble detection strategy using price-to-SMA ratio. Identifies
#   potential price bubbles when price exceeds 1.05x the 50-day SMA. Takes
#   profit at 1.08x ratio, exits if price falls back below SMA.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BubbleAlgorithm(QCAlgorithm):
    """Bubble/momentum strategy using price-to-SMA ratio."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._sma50 = self.sma(self.symbol, 50, Resolution.DAILY)
        self._sma200 = self.sma(self.symbol, 200, Resolution.DAILY)

    def on_data(self, data):
        if not self._sma50.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        sma50 = self._sma50.current.value
        ratio = price / sma50 if sma50 > 0 else 1

        if not self.portfolio.invested:
            # Enter on moderate uptrend (not too extended)
            if 1.0 < ratio < 1.05:
                self.set_holdings(self.symbol, 1)
        else:
            # Take profit if over-extended or stop if under SMA
            if ratio > 1.08 or price < sma50:
                self.liquidate()
