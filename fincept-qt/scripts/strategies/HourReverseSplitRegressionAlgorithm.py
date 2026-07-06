# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-690AD750
# Category: Regression Test
# Description: Regression test for consistency of hour data over a reverse split event in US equities
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression test for consistency of hour data over a reverse split event in US equities.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="regression test" />
class HourReverseSplitRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 11, 7)
        self.set_end_date(2013, 11, 8)
        self.set_cash(100000)
        self.set_benchmark(lambda x: 0)

        self._symbol = self.add_equity("VXX.1", Resolution.HOUR).symbol
    
    def on_data(self, slice):
        if slice.bars.count == 0: return
        if (not self.portfolio.invested) and self.time.date() == self.end_date.date():
            self.buy(self._symbol, 1)
