# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-F72D9CA4
# Category: Regression Test
# Description: Regression algorithm to test we can specify a custom benchmark model, and override some of its methods
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from CustomBrokerageModelRegressionAlgorithm import CustomBrokerageModel

### <summary>
### Regression algorithm to test we can specify a custom benchmark model, and override some of its methods
### </summary>
class CustomBenchmarkRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)
        self.set_brokerage_model(CustomBrokerageModelWithCustomBenchmark())
        self.add_equity("SPY", Resolution.DAILY)
        self.update_request_submitted = False

    def on_data(self, slice):
        benchmark = self.benchmark.evaluate(self.time)
        if (self.time.day % 2 == 0) and (benchmark != 1):
            raise Exception(f"Benchmark should be 1, but was {benchmark}")

        if (self.time.day % 2 == 1) and (benchmark != 2):
            raise Exception(f"Benchmark should be 2, but was {benchmark}")

class CustomBenchmark:
    def evaluate(self, time):
        if time.day % 2 == 0:
            return 1
        else:
            return 2

class CustomBrokerageModelWithCustomBenchmark(CustomBrokerageModel):
    def get_benchmark(self, securities):
        return CustomBenchmark()
