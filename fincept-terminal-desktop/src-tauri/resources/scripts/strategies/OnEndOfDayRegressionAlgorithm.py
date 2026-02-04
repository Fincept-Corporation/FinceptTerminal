# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5B1C96E9
# Category: Regression Test
# Description: Test algorithm verifying OnEndOfDay callbacks are called as expected. See GH issue 2865.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class OnEndOfDayRegressionAlgorithm(QCAlgorithm):
    '''Test algorithm verifying OnEndOfDay callbacks are called as expected. See GH issue 2865.'''
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10,11)
        self.set_cash(100000)

        self._spy_symbol = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        self._bac_symbol = Symbol.create("BAC", SecurityType.EQUITY, Market.USA)
        self._ibm_symbol = Symbol.create("IBM", SecurityType.EQUITY, Market.USA)
        self._on_end_of_day_spy_call_count = 0
        self._on_end_of_day_bac_call_count = 0
        self._on_end_of_day_ibm_call_count = 0

        self.add_universe('my_universe_name', self.selection)

    def selection(self, time):
        if time.day == 8:
            return [self._spy_symbol.value, self._ibm_symbol.value]
        return [self._spy_symbol.value]

    def on_end_of_day(self, symbol):
        '''We expect it to be called on each day after the first selection process
        happens and the algorithm has a security in it
        '''
        if symbol == self._spy_symbol:
            if self._on_end_of_day_spy_call_count == 0:
                # just the first time
                self.set_holdings(self._spy_symbol, 0.5)
                self.add_equity("BAC")
            self._on_end_of_day_spy_call_count += 1
        if symbol == self._bac_symbol:
            if self._on_end_of_day_bac_call_count == 0:
                # just the first time
                self.set_holdings(self._bac_symbol, 0.5)
            self._on_end_of_day_bac_call_count += 1
        if symbol == self._ibm_symbol:
            self._on_end_of_day_ibm_call_count += 1

        self.log("OnEndOfDay() called: " + str(self.utc_time)
                + ". SPY count " + str(self._on_end_of_day_spy_call_count)
                + ". BAC count " + str(self._on_end_of_day_bac_call_count)
                + ". IBM count " + str(self._on_end_of_day_ibm_call_count))

    def on_end_of_algorithm(self):
        '''Assert expected behavior'''
        if self._on_end_of_day_spy_call_count != 5:
            raise ValueError("OnEndOfDay(SPY) unexpected count call " + str(self._on_end_of_day_spy_call_count))
        if self._on_end_of_day_bac_call_count != 4:
            raise ValueError("OnEndOfDay(BAC) unexpected count call " + str(self._on_end_of_day_bac_call_count))
        if self._on_end_of_day_ibm_call_count != 1:
            raise ValueError("OnEndOfDay(IBM) unexpected count call " + str(self._on_end_of_day_ibm_call_count))
