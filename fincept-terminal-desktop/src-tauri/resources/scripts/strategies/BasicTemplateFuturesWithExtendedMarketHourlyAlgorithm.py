# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-006DF51F
# Category: Futures
# Description: Momentum-based strategy adapted from futures template. Buys when
#   price crosses above 20-day SMA, sells when crossing below. Originally designed
#   for futures contracts, adapted for equity backtesting.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicTemplateFuturesWithExtendedMarketHourlyAlgorithm(QCAlgorithm):
    """SMA trend-following strategy (adapted from futures template)."""

    def initialize(self):
        self.set_start_date(2013, 10, 8)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)

        self.symbol = 'SPY'
        self.add_equity(self.symbol, Resolution.DAILY)

        self._sma = self.sma(self.symbol, 20, Resolution.DAILY)
        self._previous_invested = False

    def on_data(self, data):
        if not self._sma.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close

        if not self.portfolio.invested and price > self._sma.current.value:
            self.set_holdings(self.symbol, 0.9)
        elif self.portfolio.invested and price < self._sma.current.value:
            self.liquidate()
