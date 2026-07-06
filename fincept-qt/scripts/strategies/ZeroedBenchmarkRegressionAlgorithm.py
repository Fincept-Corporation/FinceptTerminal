# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-F69688F5
# Category: Regression Test
# Description: Regression algorithm to test zeroed benchmark through BrokerageModel override
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm to test zeroed benchmark through BrokerageModel override
### </summary>
### <meta name="tag" content="regression test" />
class ZeroedBenchmarkRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_cash(100000)
        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,8)

        # Add Equity
        self.add_equity("SPY", Resolution.HOUR)

        # Use our Test Brokerage Model with zerod default benchmark
        self.set_brokerage_model(TestBrokerageModel())

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)

class TestBrokerageModel(DefaultBrokerageModel):

    def get_benchmark(self, securities):
        return FuncBenchmark(self.func)

    def func(self, datetime):
        return 0
