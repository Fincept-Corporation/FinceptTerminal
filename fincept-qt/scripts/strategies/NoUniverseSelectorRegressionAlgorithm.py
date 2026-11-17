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
### Custom data universe selection regression algorithm asserting it's behavior. See GH issue #6396
### </summary>
class NoUniverseSelectorRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2014, 3, 24)
        self.set_end_date(2014, 3, 31)

        self.universe_settings.resolution = Resolution.DAILY;
        self.add_universe(CoarseFundamental)
        self.changes = None

    def on_data(self, data):
        # if we have no changes, do nothing
        if not self.changes: return

        # liquidate removed securities
        for security in self.changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        active_and_with_data_securities = sum(x.value.has_data for x in self.active_securities)
        # we want 1/N allocation in each security in our universe
        for security in self.changes.added_securities:
            if security.has_data:
                self.set_holdings(security.symbol, 1 / active_and_with_data_securities)
        self.changes = None

    def on_securities_changed(self, changes):
        self.changes = changes
