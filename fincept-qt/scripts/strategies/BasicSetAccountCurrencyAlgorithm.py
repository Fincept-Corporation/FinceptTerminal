# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-07DF2AB7
# Category: General Strategy
# Description: Simple buy-and-hold strategy demonstrating account setup and
#   position management. Invests 100% in SPY when not invested. Originally
#   demonstrated multi-currency account configuration.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicSetAccountCurrencyAlgorithm(QCAlgorithm):
    """Buy-and-hold strategy demonstrating account setup."""

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
