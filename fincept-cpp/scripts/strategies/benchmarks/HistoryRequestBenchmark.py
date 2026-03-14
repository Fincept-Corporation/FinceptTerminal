# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0C67D5F6
# Category: Benchmark
# Description: Buy-and-hold benchmark strategy. Invests 100% in SPY on first
#   data point and holds for the entire period. Used as a performance baseline
#   for comparing active strategies.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class HistoryRequestBenchmark(QCAlgorithm):
    """Buy-and-hold benchmark for performance comparison."""

    def initialize(self):
        self.set_start_date(2023, 1, 1)
        self.set_end_date(2024, 1, 1)
        self.set_cash(100000)

        self.symbol = "SPY"
        self.add_equity(self.symbol, Resolution.DAILY)

    def on_data(self, data):
        if self.symbol not in data:
            return
        if not self.portfolio.invested:
            self.set_holdings(self.symbol, 1)
