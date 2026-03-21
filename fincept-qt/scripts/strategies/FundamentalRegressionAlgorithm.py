# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-4A7607D6
# Category: Regression Test
# Description: Demonstration of how to define a universe using the fundamental data
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of how to define a universe using the fundamental data
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
### <meta name="tag" content="regression test" />
class FundamentalRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 3, 26)
        self.set_end_date(2014, 4, 7)

        self.universe_settings.resolution = Resolution.DAILY

        self._universe = self.add_universe(self.selection_function)

        # before we add any symbol
        self.assert_fundamental_universe_data()

        self.add_equity("SPY")
        self.add_equity("AAPL")

        # Request fundamental data for symbols at current algorithm time
        ibm = Symbol.create("IBM", SecurityType.EQUITY, Market.USA)
        ibm_fundamental = self.fundamentals(ibm)
        if self.time != self.start_date or self.time != ibm_fundamental.end_time:
            raise ValueError(f"Unexpected Fundamental time {ibm_fundamental.end_time}")
        if ibm_fundamental.price == 0:
            raise ValueError(f"Unexpected Fundamental IBM price!")
        nb = Symbol.create("NB", SecurityType.EQUITY, Market.USA)
        fundamentals = self.fundamentals([ nb, ibm ])
        if len(fundamentals) != 2:
            raise ValueError(f"Unexpected Fundamental count {len(fundamentals)}! Expected 2")

        # Request historical fundamental data for symbols
        history = self.history(Fundamental, timedelta(days=2))
        if len(history) != 4:
            raise ValueError(f"Unexpected Fundamental history count {len(history)}! Expected 4")

        for ticker in [ "AAPL", "SPY" ]:
            data = history.loc[ticker]
            if data["value"][0] == 0:
                raise ValueError(f"Unexpected {data} fundamental data")
        if Object.reference_equals(data.earningreports.iloc[0], data.earningreports.iloc[1]):
            raise ValueError(f"Unexpected fundamental data instance duplication")
        if data.earningreports.iloc[0]._time_provider.get_utc_now() == data.earningreports.iloc[1]._time_provider.get_utc_now():
            raise ValueError(f"Unexpected fundamental data instance duplication")

        self.assert_fundamental_universe_data()

        self.changes = None
        self.number_of_symbols_fundamental = 2

    def assert_fundamental_universe_data(self):
        # Case A
        universe_data = self.history(self._universe.data_type, [self._universe.symbol], timedelta(days=2), flatten=True)
        self.assert_fundamental_history(universe_data, "A")

        # Case B (sugar on A)
        universe_data_per_time = self.history(self._universe, timedelta(days=2), flatten=True)
        self.assert_fundamental_history(universe_data_per_time, "B")

        # Case C: Passing through the unvierse type and symbol
        enumerable_of_data_dictionary = self.history[self._universe.data_type]([self._universe.symbol], 100)
        for selection_collection_for_a_day in enumerable_of_data_dictionary:
            self.assert_fundamental_enumerator(selection_collection_for_a_day[self._universe.symbol], "C")

    def assert_fundamental_history(self, df, case_name):
        dates = df.index.get_level_values('time').unique()
        if dates.shape[0] != 2:
            raise ValueError(f"Unexpected Fundamental universe dates count {dates.shape[0]}! Expected 2")

        for date in dates:
            sub_df = df.loc[date]
            if sub_df.shape[0] < 7000:
                raise ValueError(f"Unexpected historical Fundamentals data count {sub_df.shape[0]} case {case_name}! Expected > 7000")

    def assert_fundamental_enumerator(self, enumerable, case_name):
        data_point_count = 0
        for fundamental in enumerable:
            data_point_count += 1
            if type(fundamental) is not Fundamental:
                raise ValueError(f"Unexpected Fundamentals data type {type(fundamental)} case {case_name}! {str(fundamental)}")
        if data_point_count < 7000:
            raise ValueError(f"Unexpected historical Fundamentals data count {data_point_count} case {case_name}! Expected > 7000")

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
