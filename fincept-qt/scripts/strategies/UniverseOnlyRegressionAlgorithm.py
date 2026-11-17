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
### Asserts that algorithms can be universe-only, that is, universe selection is performed even if the ETF security is not explicitly added.
### Reproduces https://github.com/QuantConnect/Lean/issues/7473
### </summary>
class UniverseOnlyRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 12, 1)
        self.set_end_date(2020, 12, 12)
        self.set_cash(100000)

        self.universe_settings.resolution = Resolution.DAILY

        # Add universe without a security added
        self.add_universe(self.universe.etf("GDVD", self.universe_settings, self.filter_universe))

        self.selection_done = False

    def filter_universe(self, constituents: List[ETFConstituentData]) -> List[Symbol]:
        self.selection_done = True
        return [x.symbol for x in constituents]

    def on_end_of_algorithm(self):
        if not self.selection_done:
            raise Exception("Universe selection was not performed")
