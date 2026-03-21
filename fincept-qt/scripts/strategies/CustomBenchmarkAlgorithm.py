# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-390AE467
# Category: Benchmark
# Description: Shows how to set a custom benchmark for you algorithms
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Shows how to set a custom benchmark for you algorithms
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="benchmarks" />
class CustomBenchmarkAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY", Resolution.SECOND)
        
        # Disabling the benchmark / setting to a fixed value 
        # self.set_benchmark(lambda x: 0)
        
        # Set the benchmark to AAPL US Equity
        self.set_benchmark(Symbol.create("AAPL", SecurityType.EQUITY, Market.USA))

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)
            self.debug("Purchased Stock")

        tuple_result = SymbolCache.try_get_symbol("AAPL", None)
        if tuple_result[0]:
            raise Exception("Benchmark Symbol is not expected to be added to the Symbol cache")
