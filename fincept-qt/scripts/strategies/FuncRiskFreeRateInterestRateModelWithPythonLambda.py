# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1C8EF029
# Category: Risk Management
# Description: Asserts we can use a Python lambda function as a FuncRiskFreeRateInterestRateModel
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Asserts we can use a Python lambda function as a FuncRiskFreeRateInterestRateModel
### </summary>
class FuncRiskFreeRateInterestRateModelWithPythonLambda(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2020, 5, 28)
        self.set_end_date(2020, 6, 28)

        self.add_equity("SPY", Resolution.DAILY)
        self.model = FuncRiskFreeRateInterestRateModel(lambda dt: 1 if dt.date != datetime(2020, 5, 28) else 0)

    def on_data(self, slice):
        if self.time.date == datetime(2020, 5, 28) and self.model.get_interest_rate(self.time) != 0:
            raise Exception(f"Risk free interest rate should be 0, but was {self.model.get_interest_rate(self.time)}")
        elif self.time.date != datetime(2020, 5, 28) and self.model.get_interest_rate(self.time) != 1:
            raise Exception(f"Risk free interest rate should be 1, but was {self.model.get_interest_rate(self.time)}")
