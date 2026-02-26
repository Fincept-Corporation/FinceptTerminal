# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1855F876
# Category: Options
# Description: Intraday-inspired reversal strategy. Uses short-period RSI (7)
#   combined with EMA filter. Buys on deep oversold (RSI < 25) when above
#   EMA. Quick exit on RSI > 55 for short-term mean reversion captures.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class FutureOptionBuySellCallIntradayRegressionAlgorithm(QCAlgorithm):
    """Short-period RSI mean-reversion with EMA filter."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._rsi = self.rsi(self.symbol, 7, Resolution.DAILY)
        self._ema = self.ema(self.symbol, 50, Resolution.DAILY)

    def on_data(self, data):
        if not self._rsi.is_ready or not self._ema.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        rsi_val = self._rsi.current.value
        ema_val = self._ema.current.value

        if not self.portfolio.invested:
            # Oversold bounce near long-term trend
            if rsi_val < 35 and price > ema_val:
                self.set_holdings(self.symbol, 1)
        else:
            # Quick exit on RSI recovery or trend break
            if rsi_val > 65 or price < ema_val:
                self.liquidate()
