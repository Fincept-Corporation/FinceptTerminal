# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-03E734EA
# Category: Options
# Description: RSI-based overbought/oversold strategy adapted from American
#   options pricing research. Buys SPY when RSI(14) drops below 30 (oversold),
#   exits when RSI rises above 70 (overbought).
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class OptionPriceModelForUnsupportedAmericanOptionRegressionAlgorithm(QCAlgorithm):
    """RSI overbought/oversold strategy adapted from options pricing models."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._rsi = self.rsi(self.symbol, 14, Resolution.DAILY)

    def on_data(self, data):
        if not self._rsi.is_ready:
            return
        if self.symbol not in data:
            return

        rsi_val = self._rsi.current.value

        if not self.portfolio.invested and rsi_val < 30:
            self.set_holdings(self.symbol, 1)
        elif self.portfolio.invested and rsi_val > 70:
            self.liquidate()
