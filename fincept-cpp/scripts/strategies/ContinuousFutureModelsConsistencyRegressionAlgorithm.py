# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E27B6A38
# Category: Futures
# Description: Regression algorithm asserting that when setting custom models for canonical future, a one-time warning is sent infor...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from OptionModelsConsistencyRegressionAlgorithm import OptionModelsConsistencyRegressionAlgorithm

### <summary>
### Regression algorithm asserting that when setting custom models for canonical future, a one-time warning is sent
### informing the user that the contracts models are different (not the custom ones).
### </summary>
class ContinuousFutureModelsConsistencyRegressionAlgorithm(OptionModelsConsistencyRegressionAlgorithm):

    def initialize_algorithm(self) -> Security:
        self.set_start_date(2013, 7, 1)
        self.set_end_date(2014, 1, 1)

        continuous_contract = self.add_future(Futures.Indices.SP_500_E_MINI,
                                             data_normalization_mode=DataNormalizationMode.BACKWARDS_PANAMA_CANAL,
                                             data_mapping_mode=DataMappingMode.OPEN_INTEREST,
                                             contract_depth_offset=1)

        return continuous_contract
