# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-8C620280
# Category: Options
# Description: Regression algorithm asserting we can specify a custom option assignment
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from QuantConnect.Algorithm.CSharp import *

### <summary>
### Regression algorithm asserting we can specify a custom option assignment
### </summary>
class CustomOptionAssignmentRegressionAlgorithm(OptionAssignmentRegressionAlgorithm):
    def initialize(self):
        self.set_security_initializer(self.custom_security_initializer)
        super().initialize()

    def custom_security_initializer(self, security):
        if Extensions.is_option(security.symbol.security_type):
            # we have to be 10% in the money to get assigned
            security.set_option_assignment_model(PyCustomOptionAssignmentModel(0.1))

    def on_data(self, data):
        super().on_data(data)

class PyCustomOptionAssignmentModel(DefaultOptionAssignmentModel):
    def __init__(self, required_in_the_money_percent):
        super().__init__(required_in_the_money_percent)

    def get_assignment(self, parameters):
        result = super().get_assignment(parameters)
        result.tag = "Custom Option Assignment"
        return result
