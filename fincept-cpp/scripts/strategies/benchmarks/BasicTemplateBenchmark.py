# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-B103DBA0
# Category: Benchmark
# Description: Benchmark Algorithm: The minimalist basic template algorithm benchmark strategy
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Benchmark Algorithm: The minimalist basic template algorithm benchmark strategy.
### </summary>
### <remarks>
### All new projects in the cloud are created with the basic template algorithm. It uses a minute algorithm
### </remarks>
class BasicTemplateBenchmark(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2000, 1, 1)
        self.set_end_date(2022, 1, 1)
        self.set_benchmark(lambda x: 1)
        self.add_equity("SPY")
    
    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)
            self.debug("Purchased Stock")
