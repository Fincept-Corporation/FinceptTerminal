# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-4F55CFD9
# Category: Regression Test
# Description: Regression algorithm for testing parameterized regression algorithms get valid parameters
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm for testing parameterized regression algorithms get valid parameters.
### </summary>
class GetParameterRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.check_parameter(None, self.get_parameter("non-existing"), "GetParameter(\"non-existing\")")
        self.check_parameter("100", self.get_parameter("non-existing", "100"), "GetParameter(\"non-existing\", \"100\")")
        self.check_parameter(100, self.get_parameter("non-existing", 100), "GetParameter(\"non-existing\", 100)")
        self.check_parameter(100.0, self.get_parameter("non-existing", 100.0), "GetParameter(\"non-existing\", 100.0)")

        self.check_parameter("10", self.get_parameter("ema-fast"), "GetParameter(\"ema-fast\")")
        self.check_parameter(10, self.get_parameter("ema-fast", 100), "GetParameter(\"ema-fast\", 100)")
        self.check_parameter(10.0, self.get_parameter("ema-fast", 100.0), "GetParameter(\"ema-fast\", 100.0)")

        self.quit()

    def check_parameter(self, expected, actual, call):
        if expected == None and actual != None:
            raise Exception(f"{call} should have returned null but returned {actual} ({type(actual)})")

        if expected != None and actual == None:
            raise Exception(f"{call} should have returned {expected} ({type(expected)}) but returned null")

        if expected != None and actual != None and type(expected) != type(actual) or expected != actual:
            raise Exception(f"{call} should have returned {expected} ({type(expected)}) but returned {actual} ({type(actual)})")
