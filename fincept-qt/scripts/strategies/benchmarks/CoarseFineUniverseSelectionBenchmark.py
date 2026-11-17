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

class CoarseFineUniverseSelectionBenchmark(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2017, 11, 1)
        self.set_end_date(2018, 3, 1)
        self.set_cash(50000)

        self.universe_settings.resolution = Resolution.MINUTE

        self.add_universe(self.coarse_selection_function, self.fine_selection_function)

        self.number_of_symbols = 150
        self.number_of_symbols_fine = 40
        self._changes = None

    # sort the data by daily dollar volume and take the top 'NumberOfSymbols'
    def coarse_selection_function(self, coarse):

        selected = [x for x in coarse if (x.has_fundamental_data)]
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted(selected, key=lambda x: x.dollar_volume, reverse=True)

        # return the symbol objects of the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_dollar_volume[:self.number_of_symbols] ]

    # sort the data by P/E ratio and take the top 'NumberOfSymbolsFine'
    def fine_selection_function(self, fine):
        # sort descending by P/E ratio
        sorted_by_pe_ratio = sorted(fine, key=lambda x: x.valuation_ratios.pe_ratio, reverse=True)
        # take the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_pe_ratio[:self.number_of_symbols_fine] ]

    def on_data(self, data):
        # if we have no changes, do nothing
        if self._changes is None: return

        # liquidate removed securities
        for security in self._changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        for security in self._changes.added_securities:
            self.set_holdings(security.symbol, 0.02)
        self._changes = None

    def on_securities_changed(self, changes):
        self._changes = changes
