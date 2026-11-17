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
from System import *

### <summary>
### Regression algorithm illustrating how to request history data for different data mapping modes.
### </summary>
class HistoryWithDifferentDataMappingModeRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 6)
        self.set_end_date(2014, 1, 1)
        self._continuous_contract_symbol = self.add_future(Futures.Indices.SP_500_E_MINI, Resolution.DAILY).symbol

    def on_end_of_algorithm(self):
        data_mapping_modes = [DataMappingMode(x) for x in Enum.get_values(DataMappingMode)]
        history_results = [
            self.history([self._continuous_contract_symbol], self.start_date, self.end_date, Resolution.DAILY, data_mapping_mode=data_mapping_mode)
                .droplevel(0, axis=0)
                .loc[self._continuous_contract_symbol]
                .close
            for data_mapping_mode in data_mapping_modes
        ]

        if any(x.size != history_results[0].size for x in history_results):
            raise Exception("History results bar count did not match")

        # Check that close prices at each time are different for different data mapping modes
        for j in range(history_results[0].size):
            close_prices = set(history_results[i][j] for i in range(len(history_results)))
            if len(close_prices) != len(data_mapping_modes):
                raise Exception("History results close prices should have been different for each data mapping mode at each time")
