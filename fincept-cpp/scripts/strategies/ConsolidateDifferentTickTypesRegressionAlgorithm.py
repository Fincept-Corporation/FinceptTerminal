# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C054C3D1
# Category: Data Consolidation
# Description: This algorithm asserts we can consolidate Tick data with different tick types
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This algorithm asserts we can consolidate Tick data with different tick types
### </summary>
class ConsolidateDifferentTickTypesRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 6)
        self.set_end_date(2013, 10, 7)
        self.set_cash(1000000)

        equity = self.add_equity("SPY", Resolution.TICK, Market.USA)
        quote_consolidator = self.consolidate(equity.symbol, Resolution.TICK, TickType.QUOTE, lambda tick : self.on_quote_tick(tick))
        self.there_is_at_least_one_quote_tick = False

        trade_consolidator = self.consolidate(equity.symbol, Resolution.TICK, TickType.TRADE, lambda tick : self.on_trade_tick(tick))
        self.there_is_at_least_one_trade_tick = False

    def on_quote_tick(self, tick):
        self.there_is_at_least_one_quote_tick = True
        if tick.tick_type != TickType.QUOTE:
            raise Exception(f"The type of the tick should be Quote, but was {tick.tick_type}")

    def on_trade_tick(self, tick):
        self.there_is_at_least_one_trade_tick = True
        if tick.tick_type != TickType.TRADE:
            raise Exception(f"The type of the tick should be Trade, but was {tick.tick_type}")

    def on_end_of_algorithm(self):
        if not self.there_is_at_least_one_quote_tick:
            raise Exception(f"There should have been at least one tick in OnQuoteTick() method, but there wasn't")

        if not self.there_is_at_least_one_trade_tick:
            raise Exception(f"There should have been at least one tick in OnTradeTick() method, but there wasn't")

