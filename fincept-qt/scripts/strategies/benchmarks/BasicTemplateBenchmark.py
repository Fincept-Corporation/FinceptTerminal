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

### <summary>
### Benchmark Algorithm: The minimalist basic template algorithm benchmark strategy.
### </summary>
### <remarks>
### All new projects in the cloud are created with the basic template algorithm. It uses a minute algorithm
### </remarks>
class BasicTemplateBenchmark(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2000, 1, 1)
        self.set_end_date(2022, 1, 1)
        self.set_benchmark(lambda x: 1)
        self.add_equity("SPY")
    
    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)
            self.debug("Purchased Stock")
