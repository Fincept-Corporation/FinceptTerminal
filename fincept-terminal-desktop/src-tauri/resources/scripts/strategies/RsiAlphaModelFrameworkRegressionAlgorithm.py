# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-2CBD0408
# Category: Alpha Model
# Description: Regression algorithm to assert the behavior of <see cref="RsiAlphaModel"/>
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from BaseFrameworkRegressionAlgorithm import BaseFrameworkRegressionAlgorithm
from Alphas.RsiAlphaModel import RsiAlphaModel

### <summary>
### Regression algorithm to assert the behavior of <see cref="RsiAlphaModel"/>.
### </summary>
class RsiAlphaModelFrameworkRegressionAlgorithm(BaseFrameworkRegressionAlgorithm):

    def initialize(self):
        super().initialize()
        self.set_alpha(RsiAlphaModel())

    def on_end_of_algorithm(self):
        # We have removed all securities from the universe. The Alpha Model should remove the consolidator
        consolidator_count = sum([s.consolidators.count for s in self.subscription_manager.subscriptions])
        if consolidator_count > 0:
            raise Exception(f"The number of consolidators should be zero. Actual: {consolidator_count}")
