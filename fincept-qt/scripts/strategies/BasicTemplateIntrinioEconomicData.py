# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-13F865B8
# Category: Template
# Description: EMA trend-following strategy inspired by economic data analysis.
#   Buys when price is above 20-day EMA with positive momentum, sells when
#   price drops below EMA or momentum turns negative.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicTemplateIntrinioEconomicData(QCAlgorithm):
    """EMA trend-following with momentum confirmation."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

        self._ema = self.ema(self.symbol, 20, Resolution.DAILY)
        self._mom = self.momp(self.symbol, 10, Resolution.DAILY)

    def on_data(self, data):
        if not self._ema.is_ready or not self._mom.is_ready:
            return
        if self.symbol not in data:
            return

        price = data[self.symbol].close
        ema_val = self._ema.current.value
        mom_val = self._mom.current.value

        if not self.portfolio.invested:
            if price > ema_val and mom_val > 0:
                self.set_holdings(self.symbol, 1)
        else:
            if price < ema_val or mom_val < -2:
                self.liquidate()
