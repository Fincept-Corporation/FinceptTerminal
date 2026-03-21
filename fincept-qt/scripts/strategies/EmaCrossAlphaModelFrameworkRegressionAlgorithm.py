# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A3A4BE5B
# Category: Alpha Model
# Description: Regression algorithm to assert the behavior of <see cref="EmaCrossAlphaModel"/>
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from BaseFrameworkRegressionAlgorithm import BaseFrameworkRegressionAlgorithm
from Alphas.EmaCrossAlphaModel import EmaCrossAlphaModel

### <summary>
### Regression algorithm to assert the behavior of <see cref="EmaCrossAlphaModel"/>.
### </summary>
class EmaCrossAlphaModelFrameworkRegressionAlgorithm(BaseFrameworkRegressionAlgorithm):

    def initialize(self):
        super().initialize()
        self.set_alpha(EmaCrossAlphaModel())

    def on_end_of_algorithm(self):
        pass
