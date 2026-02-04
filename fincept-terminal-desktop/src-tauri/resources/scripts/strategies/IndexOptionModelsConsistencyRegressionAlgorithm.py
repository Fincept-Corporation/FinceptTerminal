# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-02F5AEAE
# Category: Options
# Description: Algorithm asserting that when setting custom models for canonical index options, a one-time warning is sent informing...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from OptionModelsConsistencyRegressionAlgorithm import OptionModelsConsistencyRegressionAlgorithm

### <summary>
### Algorithm asserting that when setting custom models for canonical index options, a one-time warning is sent
### informing the user that the contracts models are different (not the custom ones).
### </summary>
class IndexOptionModelsConsistencyRegressionAlgorithm(OptionModelsConsistencyRegressionAlgorithm):

    def initialize_algorithm(self) -> Security:
        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 1, 5)

        index = self.add_index("SPX", Resolution.MINUTE)
        option = self.add_index_option(index.symbol, "SPX", Resolution.MINUTE)
        option.set_filter(lambda u: u.strikes(-5, +5).expiration(0, 360))

        return option
