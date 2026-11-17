# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

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
