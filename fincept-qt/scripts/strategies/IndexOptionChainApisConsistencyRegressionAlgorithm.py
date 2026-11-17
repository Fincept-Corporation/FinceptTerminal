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
