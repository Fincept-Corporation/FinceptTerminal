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
from System.Collections.Generic import List

### <summary>
### Demonstration of using coarse and fine universe selection together to filter down a smaller universe of stocks.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
### <meta name="tag" content="fine universes" />
class CoarseFineFundamentalComboAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2014,1,1)  #Set Start Date
        self.set_end_date(2015,1,1)    #Set End Date
        self.set_cash(50000)            #Set Strategy Cash

        # what resolution should the data *added* to the universe be?
        self.universe_settings.resolution = Resolution.DAILY

        # this add universe method accepts two parameters:
        # - coarse selection function: accepts an IEnumerable<CoarseFundamental> and returns an IEnumerable<Symbol>
        # - fine selection function: accepts an IEnumerable<FineFundamental> and returns an IEnumerable<Symbol>
        self.add_universe(self.coarse_selection_function, self.fine_selection_function)

        self.__number_of_symbols = 5
        self.__number_of_symbols_fine = 2
        self._changes = None


    # sort the data by daily dollar volume and take the top 'NumberOfSymbols'
    def coarse_selection_function(self, coarse):
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted(coarse, key=lambda x: x.dollar_volume, reverse=True)

        # return the symbol objects of the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_dollar_volume[:self.__number_of_symbols] ]

    # sort the data by P/E ratio and take the top 'NumberOfSymbolsFine'
    def fine_selection_function(self, fine):
        # sort descending by P/E ratio
        sorted_by_pe_ratio = sorted(fine, key=lambda x: x.valuation_ratios.pe_ratio, reverse=True)

        # take the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_pe_ratio[:self.__number_of_symbols_fine] ]

    def on_data(self, data):
        # if we have no changes, do nothing
        if self._changes is None: return

        # liquidate removed securities
        for security in self._changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        # we want 20% allocation in each security in our universe
        for security in self._changes.added_securities:
            self.set_holdings(security.symbol, 0.2)

        self._changes = None


    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        self._changes = changes
