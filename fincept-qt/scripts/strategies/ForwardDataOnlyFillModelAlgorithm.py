# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0212DBCD
# Category: General Strategy
# Description: Weekly rebalancing strategy. Buys SPY at the start of each week
#   if not already invested. Originally demonstrated custom fill models; adapted
#   to use simple weekly entry logic with standard fills.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class ForwardDataOnlyFillModelAlgorithm(QCAlgorithm):
    """Weekly rebalancing strategy: enters SPY position each Monday."""

    def initialize(self):
        self.set_start_date(2013, 10, 1)
        self.set_end_date(2013, 10, 31)
        self.set_cash(100000)

        self.add_equity("SPY", Resolution.DAILY)
        self._last_trade_week = -1

    def on_data(self, data):
        if "SPY" not in data:
            return

        current_week = self.time.isocalendar()[1]

        # Trade once per week
        if current_week != self._last_trade_week:
            self._last_trade_week = current_week
            if not self.portfolio.invested:
                self.set_holdings("SPY", 1)
