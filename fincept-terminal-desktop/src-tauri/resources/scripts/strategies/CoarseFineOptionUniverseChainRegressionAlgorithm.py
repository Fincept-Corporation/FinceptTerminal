# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5542D978
# Category: Options
# Description: Demonstration of how to chain a coarse and fine universe selection with an option chain universe selection model that...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of how to chain a coarse and fine universe selection with an option chain universe selection model
### that will add and remove an'OptionChainUniverse' for each symbol selected on fine
### </summary>
class CoarseFineOptionUniverseChainRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2014,6,4)
        # TWX is selected the 4th and 5th and aapl after that.
        # If the algo ends on the 6th, TWX subscriptions will not be removed before OnEndOfAlgorithm is called:
        #   - 6th: AAPL is selected, TWX is removed but subscriptions are not removed because the securities are invested.
        #      - TWX and its options are liquidated.
        #   - 7th: Since options universe selection is daily now, TWX subscriptions are removed the next day (7th)
        self.set_end_date(2014,6,7)

        self.universe_settings.resolution = Resolution.MINUTE
        self._twx = Symbol.create("TWX", SecurityType.EQUITY, Market.USA)
        self._aapl = Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)
        self._last_equity_added = None
        self._changes = None
        self._option_count = 0

        universe = self.add_universe(self.coarse_selection_function, self.fine_selection_function)

        self.add_universe_options(universe, self.option_filter_function)

    def option_filter_function(self, universe):
        universe.include_weeklys().front_month()

        contracts = list()
        for contract in universe:
            if len(contracts) == 5:
                break
            contracts.append(contract)
        return universe.contracts(contracts)

    def coarse_selection_function(self, coarse):
        if self.time <= datetime(2014,6,5):
            return [ self._twx ]
        return [ self._aapl ]

    def fine_selection_function(self, fine):
        if self.time <= datetime(2014,6,5):
            return [ self._twx ]
        return [ self._aapl ]

    def on_data(self, data):
        if self._changes == None or any(security.price == 0 for security in self._changes.added_securities):
            return

        # liquidate removed securities
        for security in self._changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        for security in self._changes.added_securities:
            if not security.symbol.has_underlying:
                self._last_equity_added = security.symbol
            else:
                # options added should all match prev added security
                if security.symbol.underlying != self._last_equity_added:
                    raise ValueError(f"Unexpected symbol added {security.symbol}")
                self._option_count += 1

            self.set_holdings(security.symbol, 0.05)
        self._changes = None

    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        if self._changes == None:
            self._changes = changes
            return
        self._changes = self._changes + changes

    def on_end_of_algorithm(self):
        if self._option_count == 0:
            raise ValueError("Option universe chain did not add any option!")
