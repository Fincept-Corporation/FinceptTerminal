# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-16F5EE2E
# Category: Data Consolidation
# Description: Multi-timeframe confirmation strategy. Uses both 5-day and 20-day
#   SMAs. Enters when both short and long SMAs confirm uptrend, exits when
#   short SMA crosses below long SMA.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class ConsolidateRegressionAlgorithm(QCAlgorithm):
    """Multi-timeframe SMA confirmation strategy."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._sma5 = self.sma(self.symbol, 5, Resolution.DAILY)
        self._sma20 = self.sma(self.symbol, 20, Resolution.DAILY)
        self._sma50 = self.sma(self.symbol, 50, Resolution.DAILY)

    def on_data(self, data):
        if not self._sma50.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        s5 = self._sma5.current.value
        s20 = self._sma20.current.value
        s50 = self._sma50.current.value

        if not self.portfolio.invested:
            # All timeframes aligned bullish
            if s5 > s20 > s50 and price > s5:
                self.set_holdings(self.symbol, 1)
        else:
            # Short-term trend reversal
            if s5 < s20:
                self.liquidate()
