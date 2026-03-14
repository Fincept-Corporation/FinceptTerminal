# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C0F56022
# Category: Data Consolidation
# Description: Algorithm asserting that consolidated bars are of type `QuoteBar` when `QCAlgorithm.consolidate()` is called with `ti...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Algorithm asserting that consolidated bars are of type `QuoteBar` when `QCAlgorithm.consolidate()` is called with `tick_type=TickType.QUOTE`
### </summary>
class CorrectConsolidatedBarTypeForTickTypesAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 7)

        symbol = self.add_equity("SPY", Resolution.TICK).symbol

        self.consolidate(symbol, timedelta(minutes=1), TickType.QUOTE, self.quote_tick_consolidation_handler)
        self.consolidate(symbol, timedelta(minutes=1), TickType.TRADE, self.trade_tick_consolidation_handler)

        self.quote_tick_consolidation_handler_called = False
        self.trade_tick_consolidation_handler_called = False

    def on_data(self, slice: Slice) -> None:
        if self.time.hour > 9:
            self.quit("Early quit to save time")

    def on_end_of_algorithm(self):
        if not self.quote_tick_consolidation_handler_called:
            raise Exception("quote_tick_consolidation_handler was not called")

        if not self.trade_tick_consolidation_handler_called:
            raise Exception("trade_tick_consolidation_handler was not called")

    def quote_tick_consolidation_handler(self, consolidated_bar: QuoteBar) -> None:
        if type(consolidated_bar) != QuoteBar:
            raise Exception(f"Expected the consolidated bar to be of type {QuoteBar} but was {type(consolidated_bar)}")

        self.quote_tick_consolidation_handler_called = True

    def trade_tick_consolidation_handler(self, consolidated_bar: TradeBar) -> None:
        if type(consolidated_bar) != TradeBar:
            raise Exception(f"Expected the consolidated bar to be of type {TradeBar} but was {type(consolidated_bar)}")

        self.trade_tick_consolidation_handler_called = True
