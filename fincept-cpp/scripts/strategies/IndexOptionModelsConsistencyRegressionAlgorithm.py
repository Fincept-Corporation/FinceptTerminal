# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-02F5AEAE
# Category: Options
# Description: Volatility mean-reversion strategy inspired by options pricing
#   models. Buys when 5-day price standard deviation drops below 80% of 20-day
#   stdev (low vol regime), exits when 5-day exceeds 130% of 20-day (vol spike).
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class IndexOptionModelsConsistencyRegressionAlgorithm(QCAlgorithm):
    """Volatility mean-reversion: enters in calm markets, exits on vol spikes."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._fast_std = self.std(self.symbol, 5, Resolution.DAILY)
        self._slow_std = self.std(self.symbol, 20, Resolution.DAILY)

    def on_data(self, data):
        if not self._fast_std.is_ready or not self._slow_std.is_ready:
            return
        if self.symbol not in data:
            return

        fast_vol = self._fast_std.current.value
        slow_vol = self._slow_std.current.value

        if slow_vol == 0:
            return

        vol_ratio = fast_vol / slow_vol

        if not self.portfolio.invested and vol_ratio < 0.8:
            self.set_holdings(self.symbol, 1)
        elif self.portfolio.invested and vol_ratio > 1.3:
            self.liquidate()
