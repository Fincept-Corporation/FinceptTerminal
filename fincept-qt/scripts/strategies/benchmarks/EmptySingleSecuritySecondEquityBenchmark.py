# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9BA28BE5
# Category: Benchmark
# Description: Benchmark Algorithm: Pure processing of 1 equity second resolution with the same benchmark
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Benchmark Algorithm: Pure processing of 1 equity second resolution with the same benchmark.
### </summary>
### <remarks>
### This should eliminate the synchronization part of LEAN and focus on measuring the performance of a single datafeed and event handling system.
### </remarks>
class EmptySingleSecuritySecondEquityBenchmark(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2008, 1, 1)
        self.set_end_date(2008, 6, 1)
        self.set_benchmark(lambda x: 1)
        self.add_equity("SPY", Resolution.SECOND)
    
    def on_data(self, data):
        pass
