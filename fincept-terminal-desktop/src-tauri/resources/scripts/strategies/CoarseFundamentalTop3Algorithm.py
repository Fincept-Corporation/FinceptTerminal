# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-F95F1806
# Category: General Strategy
# Description: Demonstration of using coarse and fine universe selection together to filter down a smaller universe of stocks
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of using coarse and fine universe selection together to filter down a smaller universe of stocks.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
### <meta name="tag" content="fine universes" />
class CoarseFundamentalTop3Algorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2014,3,24)    #Set Start Date
        self.set_end_date(2014,4,7)      #Set End Date
        self.set_cash(50000)            #Set Strategy Cash

        # what resolution should the data *added* to the universe be?
        self.universe_settings.resolution = Resolution.DAILY

        # this add universe method accepts a single parameter that is a function that
        # accepts an IEnumerable<CoarseFundamental> and returns IEnumerable<Symbol>
        self.add_universe(self.coarse_selection_function)

        self.__number_of_symbols = 3
        self._changes = None


    # sort the data by daily dollar volume and take the top '__number_of_symbols'
    def coarse_selection_function(self, coarse):
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted(coarse, key=lambda x: x.dollar_volume, reverse=True)

        # return the symbol objects of the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_dollar_volume[:self.__number_of_symbols] ]


    def on_data(self, data):

        self.log(f"OnData({self.utc_time}): Keys: {', '.join([key.value for key in data.keys()])}")

        # if we have no changes, do nothing
        if self._changes is None: return

        # liquidate removed securities
        for security in self._changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        # we want 1/N allocation in each security in our universe
        for security in self._changes.added_securities:
            self.set_holdings(security.symbol, 1 / self.__number_of_symbols)

        self._changes = None


    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        self._changes = changes
        self.log(f"OnSecuritiesChanged({self.utc_time}):: {changes}")

    def on_order_event(self, fill):
        self.log(f"OnOrderEvent({self.utc_time}):: {fill}")
