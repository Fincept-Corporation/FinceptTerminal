# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5077EFC4
# Category: Regression Test
# Description: Regression algorithm asserting that tick history request includes both trade and quote data
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting that tick history request includes both trade and quote data
### </summary>
class HistoryTickRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 11)
        self.set_end_date(2013, 10, 11)

        self._symbol = self.add_equity("SPY", Resolution.TICK).symbol

    def on_end_of_algorithm(self):
        history = list(self.history[Tick](self._symbol, timedelta(days=1), Resolution.TICK))
        quotes = [x for x in history if x.tick_type == TickType.QUOTE]
        trades = [x for x in history if x.tick_type == TickType.TRADE]

        if not quotes or not trades:
            raise Exception("Expected to find at least one tick of each type (quote and trade)")
