# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0C67D5F6
# Category: Benchmark
# Description: History Request Benchmark
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class HistoryRequestBenchmark(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2010, 1, 1)
        self.set_end_date(2018, 1, 1)
        self.set_cash(10000)
        self._symbol = self.add_equity("SPY").symbol

    def on_end_of_day(self, symbol):
        minute_history = self.history([self._symbol], 60, Resolution.MINUTE)
        last_hour_high = 0
        for index, row in minute_history.loc["SPY"].iterrows():
            if last_hour_high < row["high"]:
                last_hour_high = row["high"]

        daily_history = self.history([self._symbol], 1, Resolution.DAILY).loc["SPY"].head()
        daily_history_high = daily_history["high"]
        daily_history_low = daily_history["low"]
        daily_history_open = daily_history["open"]
