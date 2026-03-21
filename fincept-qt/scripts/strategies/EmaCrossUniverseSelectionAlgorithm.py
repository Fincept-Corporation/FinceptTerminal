# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E66F50F9
# Category: Universe Selection
# Description: In this algorithm we demonstrate how to perform some technical analysis as part of your coarse fundamental universe s...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### In this algorithm we demonstrate how to perform some technical analysis as
### part of your coarse fundamental universe selection
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
class EmaCrossUniverseSelectionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2010,1,1)  #Set Start Date
        self.set_end_date(2015,1,1)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self.universe_settings.resolution = Resolution.DAILY
        self.universe_settings.leverage = 2

        self.coarse_count = 10
        self.averages = { }

        # this add universe method accepts two parameters:
        # - coarse selection function: accepts an IEnumerable<CoarseFundamental> and returns an IEnumerable<Symbol>
        self.add_universe(self.coarse_selection_function)


    # sort the data by daily dollar volume and take the top 'NumberOfSymbols'
    def coarse_selection_function(self, coarse):

        # We are going to use a dictionary to refer the object that will keep the moving averages
        for cf in coarse:
            if cf.symbol not in self.averages:
                self.averages[cf.symbol] = SymbolData(cf.symbol)

            # Updates the SymbolData object with current EOD price
            avg = self.averages[cf.symbol]
            avg.update(cf.end_time, cf.adjusted_price)

        # Filter the values of the dict: we only want up-trending securities
        values = list(filter(lambda x: x.is_uptrend, self.averages.values()))

        # Sorts the values of the dict: we want those with greater difference between the moving averages
        values.sort(key=lambda x: x.scale, reverse=True)

        for x in values[:self.coarse_count]:
            self.log('symbol: ' + str(x.symbol.value) + '  scale: ' + str(x.scale))

        # we need to return only the symbol objects
        return [ x.symbol for x in values[:self.coarse_count] ]

    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        # liquidate removed securities
        for security in changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        # we want 20% allocation in each security in our universe
        for security in changes.added_securities:
            self.set_holdings(security.symbol, 0.1)


class SymbolData(object):
    def __init__(self, symbol):
        self._symbol = symbol
        self.tolerance = 1.01
        self.fast = ExponentialMovingAverage(100)
        self.slow = ExponentialMovingAverage(300)
        self.is_uptrend = False
        self.scale = 0

    def update(self, time, value):
        if self.fast.update(time, value) and self.slow.update(time, value):
            fast = self.fast.current.value
            slow = self.slow.current.value
            self.is_uptrend = fast > slow * self.tolerance

        if self.is_uptrend:
            self.scale = (fast - slow) / ((fast + slow) / 2.0)
