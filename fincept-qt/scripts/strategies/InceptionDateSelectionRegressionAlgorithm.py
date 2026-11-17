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
### Regression algorithm to test universe additions and removals with open positions
### </summary>
### <meta name="tag" content="regression test" />
class InceptionDateSelectionRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013,10,1)
        self.set_end_date(2013,10,31)
        self.set_cash(100000)

        self.changes = None
        self.universe_settings.resolution = Resolution.HOUR

        # select IBM once a week, empty universe the other days
        self.add_universe_selection(CustomUniverseSelectionModel("my-custom-universe", lambda dt: ["IBM"] if dt.day % 7 == 0 else []))
        # Adds SPY 5 days after StartDate and keep it in Universe
        self.add_universe_selection(InceptionDateUniverseSelectionModel("spy-inception", {"SPY": self.start_date + timedelta(5)}))


    def on_data(self, slice):
        if self.changes is None:
           return

        # we'll simply go long each security we added to the universe
        for security in self.changes.added_securities:
            self.set_holdings(security.symbol, .5)

        self.changes = None

    def on_securities_changed(self, changes):
        # liquidate removed securities
        for security in changes.removed_securities:
            self.liquidate(security.symbol, "Removed from Universe")

        self.changes = changes
