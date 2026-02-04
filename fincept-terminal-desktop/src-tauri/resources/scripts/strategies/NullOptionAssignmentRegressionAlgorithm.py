# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-16C48497
# Category: Options
# Description: Regression algorithm assering we can disable automatic option assignment
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from QuantConnect.Algorithm.CSharp import *

### <summary>
### Regression algorithm assering we can disable automatic option assignment
### </summary>
class NullOptionAssignmentRegressionAlgorithm(OptionAssignmentRegressionAlgorithm):
    def initialize(self):
        self.set_security_initializer(self.custom_security_initializer)
        super().initialize()

    def on_data(self, data):
        super().on_data(data)

    def custom_security_initializer(self, security):
        if Extensions.is_option(security.symbol.security_type):
            security.set_option_assignment_model(NullOptionAssignmentModel())
