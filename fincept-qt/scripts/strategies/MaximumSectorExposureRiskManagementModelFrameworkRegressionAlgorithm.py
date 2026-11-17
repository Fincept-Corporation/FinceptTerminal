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
from BaseFrameworkRegressionAlgorithm import BaseFrameworkRegressionAlgorithm
from Risk.MaximumSectorExposureRiskManagementModel import MaximumSectorExposureRiskManagementModel

### <summary>
### Regression algorithm to assert the behavior of <see cref="MaximumSectorExposureRiskManagementModel"/>.
### </summary>
class MaximumSectorExposureRiskManagementModelFrameworkRegressionAlgorithm(BaseFrameworkRegressionAlgorithm):

    def initialize(self):
        super().initialize()
        # Set requested data resolution
        self.universe_settings.resolution = Resolution.DAILY

        self.set_start_date(2014, 2, 1)  #Set Start Date
        self.set_end_date(2014, 5, 1)    #Set End Date

        # set algorithm framework models
        tickers = [ "AAPL", "MSFT", "GOOG", "AIG", "BAC" ]
        self.set_universe_selection(FineFundamentalUniverseSelectionModel(
            lambda coarse: [ x.symbol for x in coarse if x.symbol.value in tickers ],
            lambda fine: [ x.symbol for x in fine ]
        ))

        # define risk management model such that maximum weight of a single sector be 10%
        # Number of of trades changed from 34 to 30 when using the MaximumSectorExposureRiskManagementModel
        self.set_risk_management(MaximumSectorExposureRiskManagementModel(0.1))

    def on_end_of_algorithm(self):
        pass
