# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0.  Copyright 2014 QuantConnect
# Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

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
