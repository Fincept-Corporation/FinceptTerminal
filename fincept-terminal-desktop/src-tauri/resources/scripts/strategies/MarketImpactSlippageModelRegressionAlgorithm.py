# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-26E2B07B
# Category: Regression Test
# Description: Market Impact Slippage Model Regression Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class MarketImpactSlippageModelRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 13)
        self.set_cash(10000000)

        spy = self.add_equity("SPY", Resolution.DAILY)
        aapl = self.add_equity("AAPL", Resolution.DAILY)

        spy.set_slippage_model(MarketImpactSlippageModel(self))
        aapl.set_slippage_model(MarketImpactSlippageModel(self))

    def on_data(self, data):
        self.set_holdings("SPY", 0.5)
        self.set_holdings("AAPL", -0.5)

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug(f"Price: {self.securities[order_event.symbol].price}, filled price: {order_event.fill_price}, quantity: {order_event.fill_quantity}")
