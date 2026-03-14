# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0021591C
# Category: Indicators
# Description: Ratio-based pair trading using SMA indicator extensions. Buys IBM
#   when SPY/IBM price ratio > 1, liquidates when ratio < 1. Demonstrates
#   indicator comparison for relative-value strategies.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class CustomDataIndicatorExtensionsAlgorithm(QCAlgorithm):
    """Ratio-based pair trading: buys IBM when SPY/IBM ratio > 1, exits when < 1."""

    def initialize(self):
        self.set_start_date(2014, 1, 1)
        self.set_end_date(2018, 1, 1)
        self.set_cash(25000)

        self.ibm = 'IBM'
        self.spy = 'SPY'

        self.add_equity(self.ibm, Resolution.DAILY)
        self.add_equity(self.spy, Resolution.DAILY)

        # 1-period SMA = identity (tracks closing price)
        self.ibm_sma = self.sma(self.ibm, 1, Resolution.DAILY)
        self.spy_sma = self.sma(self.spy, 1, Resolution.DAILY)

    def on_data(self, data):
        # Wait for indicators to be ready
        if not (self.ibm_sma.is_ready and self.spy_sma.is_ready):
            return

        ibm_val = self.ibm_sma.current.value
        spy_val = self.spy_sma.current.value

        if ibm_val == 0:
            return

        ratio = spy_val / ibm_val

        if not self.portfolio.invested and ratio > 1:
            self.market_order(self.ibm, 100)
        elif self.portfolio.invested and ratio < 1:
            self.liquidate()
