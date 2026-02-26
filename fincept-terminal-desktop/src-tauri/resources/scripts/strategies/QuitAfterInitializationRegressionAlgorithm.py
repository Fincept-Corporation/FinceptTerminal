# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0CB9D7FA
# Category: Regression Test
# Description: Quick-entry momentum strategy. Immediately buys SPY when price
#   is above 10-day EMA and RSI is below 70. Exits when RSI exceeds 80 or
#   price drops below EMA. Originally a regression test for init behavior.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class QuitAfterInitializationRegressionAlgorithm(QCAlgorithm):
    """Quick-entry momentum strategy with RSI filter."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._ema = self.ema(self.symbol, 10, Resolution.DAILY)
        self._rsi = self.rsi(self.symbol, 14, Resolution.DAILY)

    def on_data(self, data):
        if not self._ema.is_ready or not self._rsi.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        ema_val = self._ema.current.value
        rsi_val = self._rsi.current.value

        if not self.portfolio.invested:
            if price > ema_val and rsi_val < 70:
                self.set_holdings(self.symbol, 1)
        else:
            if rsi_val > 80 or price < ema_val:
                self.liquidate()
