# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-F859111A
# Category: Options
# Description: Regression algorithm asserting that the option chain APIs return consistent values for index options. See QCAlgorithm...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from datetime import datetime
from AlgorithmImports import *

from OptionChainApisConsistencyRegressionAlgorithm import OptionChainApisConsistencyRegressionAlgorithm

### <summary>
### Regression algorithm asserting that the option chain APIs return consistent values for index options.
### See QCAlgorithm.OptionChain(Symbol) and QCAlgorithm.OptionChainProvider
### </summary>
class IndexOptionChainApisConsistencyRegressionAlgorithm(OptionChainApisConsistencyRegressionAlgorithm):

    def get_test_date(self) -> datetime:
        return datetime(2021, 1, 4)

    def get_option(self) -> Option:
        return self.add_index_option("SPX")
