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
### Demonstration of how to define a universe using the fundamental data
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
### <meta name="tag" content="regression test" />
class FundamentalUniverseSelectionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 3, 25)
        self.set_end_date(2014, 4, 7)

        self.universe_settings.resolution = Resolution.DAILY

        self.add_equity("SPY")
        self.add_equity("AAPL")

        self.set_universe_selection(FundamentalUniverseSelectionModel(self.select))

        self.changes = None

    # return a list of three fixed symbol objects
    def selection_function(self, fundamental):
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted([x for x in fundamental if x.price > 1],
            key=lambda x: x.dollar_volume, reverse=True)

        # sort descending by P/E ratio
        sorted_by_pe_ratio = sorted(sorted_by_dollar_volume, key=lambda x: x.valuation_ratios.pe_ratio, reverse=True)

        # take the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_pe_ratio[:self.number_of_symbols_fundamental] ]

    def on_data(self, data):
        # if we have no changes, do nothing
        if self.changes is None: return

        # liquidate removed securities
        for security in self.changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)
                self.debug("Liquidated Stock: " + str(security.symbol.value))

        # we want 50% allocation in each security in our universe
        for security in self.changes.added_securities:
            self.set_holdings(security.symbol, 0.02)

        self.changes = None

    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        self.changes = changes

    def select(self, fundamental):
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted([x for x in fundamental if x.has_fundamental_data and x.price > 1],
            key=lambda x: x.dollar_volume, reverse=True)

        # sort descending by P/E ratio
        sorted_by_pe_ratio = sorted(sorted_by_dollar_volume, key=lambda x: x.valuation_ratios.pe_ratio, reverse=True)

        # take the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_pe_ratio[:2] ]
