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
from CustomSettlementModelRegressionAlgorithm import CustomSettlementModel, CustomSettlementModelRegressionAlgorithm

### <summary>
### Regression algorithm to test we can specify a custom settlement model using Security.set_settlement_model() method
### (without a custom brokerage model)
### </summary>
class SetCustomSettlementModelRegressionAlgorithm(CustomSettlementModelRegressionAlgorithm):
    def set_settlement_model(self, security):
        security.set_settlement_model(CustomSettlementModel())
