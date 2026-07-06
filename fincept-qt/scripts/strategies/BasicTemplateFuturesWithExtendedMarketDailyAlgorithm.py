# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-074CBF2C
# Category: Futures
# Description: EMA crossover strategy adapted from futures daily template.
#   Uses 10/30 EMA crossover to generate buy/sell signals. Buys when fast EMA
#   crosses above slow EMA, exits when fast crosses below.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicTemplateFuturesWithExtendedMarketDailyAlgorithm(QCAlgorithm):
    """EMA 10/30 crossover strategy (adapted from futures daily template)."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._fast_ema = self.ema(self.symbol, 10, Resolution.DAILY)
        self._slow_ema = self.ema(self.symbol, 30, Resolution.DAILY)

    def on_data(self, data):
        if not self._fast_ema.is_ready or not self._slow_ema.is_ready:
            return
        if self.symbol not in data:
            return

        fast = self._fast_ema.current.value
        slow = self._slow_ema.current.value

        if not self.portfolio.invested and fast > slow:
            self.set_holdings(self.symbol, 0.9)
        elif self.portfolio.invested and fast < slow:
            self.liquidate()
