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
from QuantConnect.Data.Custom.IconicTypes import *

class CustomDataLinkedIconicTypeAddDataOnSecuritiesChangedRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 3, 24)
        self.set_end_date(2014, 4, 7)
        self.set_cash(100000)

        self.universe_settings.resolution = Resolution.DAILY

        self.add_universe_selection(CoarseFundamentalUniverseSelectionModel(self.coarse_selector))

    def coarse_selector(self, coarse):
        return [
            Symbol.create("AAPL", SecurityType.EQUITY, Market.USA),
            Symbol.create("BAC", SecurityType.EQUITY, Market.USA),
            Symbol.create("FB", SecurityType.EQUITY, Market.USA),
            Symbol.create("GOOGL", SecurityType.EQUITY, Market.USA),
            Symbol.create("GOOG", SecurityType.EQUITY, Market.USA),
            Symbol.create("IBM", SecurityType.EQUITY, Market.USA),
        ]

    def on_data(self, data):
        if not self.portfolio.invested and len(self.transactions.get_open_orders()) == 0:
            aapl = Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)
            self.set_holdings(aapl, 0.5)

        for custom_symbol in self.custom_symbols:
            if not self.active_securities.contains_key(custom_symbol.underlying):
                raise Exception(f"Custom data undelrying ({custom_symbol.underlying}) Symbol was not found in active securities")

    def on_securities_changed(self, changes):
        iterated = False

        for added in changes.added_securities:
            if not iterated:
                self.custom_symbols = []
                iterated = True

            self.custom_symbols.append(self.add_data(LinkedData, added.symbol, Resolution.DAILY).symbol)
