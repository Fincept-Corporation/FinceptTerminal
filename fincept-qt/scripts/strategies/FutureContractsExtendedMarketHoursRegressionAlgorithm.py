# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E85797B9
# Category: Futures
# Description: This regression algorithm asserts that futures have data at extended market hours when this is enabled
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm asserts that futures have data at extended market hours when this is enabled.
### </summary>
class FutureContractsExtendedMarketHoursRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 6)
        self.set_end_date(2013, 10, 11)

        es_future_symbol = Symbol.create_future(Futures.Indices.SP_500_E_MINI, Market.CME, DateTime(2013, 12, 20))
        self._es = self.add_future_contract(es_future_symbol, Resolution.HOUR, fill_forward=True, extended_market_hours=True)

        gc_future_symbol = Symbol.create_future(Futures.Metals.GOLD, Market.COMEX, DateTime(2013, 10, 29))
        self._gc = self.add_future_contract(gc_future_symbol, Resolution.HOUR, fill_forward=True, extended_market_hours=False)

        self._es_ran_on_regular_hours = False
        self._es_ran_on_extended_hours = False
        self._gc_ran_on_regular_hours = False
        self._gc_ran_on_extended_hours = False

    def on_data(self, slice):
        slice_symbols = set(slice.keys())
        slice_symbols.update(slice.bars.keys())
        slice_symbols.update(slice.ticks.keys())
        slice_symbols.update(slice.quote_bars.keys())
        slice_symbols.update([x.canonical for x in slice_symbols])

        es_is_in_regular_hours = self._es.exchange.hours.is_open(self.time, False)
        es_is_in_extended_hours = not es_is_in_regular_hours and self._es.exchange.hours.is_open(self.time, True)
        slice_has_es_data = self._es.symbol in slice_symbols
        self._es_ran_on_regular_hours |= es_is_in_regular_hours and slice_has_es_data
        self._es_ran_on_extended_hours |= es_is_in_extended_hours and slice_has_es_data

        gc_is_in_regular_hours = self._gc.exchange.hours.is_open(self.time, False)
        gc_is_in_extended_hours = not gc_is_in_regular_hours and self._gc.exchange.hours.is_open(self.time, True)
        slice_has_gc_data = self._gc.symbol in slice_symbols
        self._gc_ran_on_regular_hours |= gc_is_in_regular_hours and slice_has_gc_data
        self._gc_ran_on_extended_hours |= gc_is_in_extended_hours and slice_has_gc_data

    def on_end_of_algorithm(self):
        if not self._es_ran_on_regular_hours:
            raise Exception(f"Algorithm should have run on regular hours for {self._es.symbol} future, which enabled extended market hours")

        if not self._es_ran_on_extended_hours:
            raise Exception(f"Algorithm should have run on extended hours for {self._es.symbol} future, which enabled extended market hours")

        if not self._gc_ran_on_regular_hours:
            raise Exception(f"Algorithm should have run on regular hours for {self._gc.symbol} future, which did not enable extended market hours")

        if self._gc_ran_on_extended_hours:
            raise Exception(f"Algorithm should have not run on extended hours for {self._gc.symbol} future, which did not enable extended market hours")
