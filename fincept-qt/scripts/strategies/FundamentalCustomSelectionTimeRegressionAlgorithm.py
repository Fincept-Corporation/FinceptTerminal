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
### Regression test algorithm for scheduled universe selection GH 3890
### </summary>
class FundamentalCustomSelectionTimeRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self._month_start_selection = 0
        self._month_end_selection = 0
        self._specific_date_selection = 0
        self._symbol = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)

        self.set_start_date(2014, 3, 25)
        self.set_end_date(2014, 5, 10)
        self.universe_settings.resolution = Resolution.DAILY

        # Test use case A
        self.add_universe(self.date_rules.month_start(), self.selection_function__month_start)

        # Test use case B
        other_settings = UniverseSettings(self.universe_settings)
        other_settings.schedule.on(self.date_rules.month_end())

        self.add_universe(FundamentalUniverse.usa(self.selection_function__month_end, other_settings))

        # Test use case C
        self.universe_settings.schedule.on(self.date_rules.on(datetime(2014, 5, 9)))
        self.add_universe(FundamentalUniverse.usa(self.selection_function__specific_date))

    def selection_function__specific_date(self, coarse):
        self._specific_date_selection += 1
        if self.time != datetime(2014, 5, 9):
            raise ValueError("SelectionFunction_SpecificDate unexpected selection: " + str(self.time))
        return [ self._symbol ]

    def selection_function__month_start(self, coarse):
        self._month_start_selection += 1
        if self._month_start_selection == 1:
            if self.time != self.start_date:
                raise ValueError("Month Start Unexpected initial selection: " + str(self.time))
        elif self.time != datetime(2014, 4, 1) and self.time != datetime(2014, 5, 1):
            raise ValueError("Month Start unexpected selection: " + str(self.time))
        return [ self._symbol ]

    def selection_function__month_end(self, coarse):
        self._month_end_selection += 1
        if self._month_end_selection == 1:
            if self.time != self.start_date:
                raise ValueError("Month End unexpected initial selection: " + str(self.time))
        elif self.time != datetime(2014, 3, 31) and self.time != datetime(2014, 4, 30):
            raise ValueError("Month End unexpected selection: " + str(self.time))
        return [ self._symbol ]

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings(self._symbol, 1)

    def on_end_of_algorithm(self):
        if self._month_end_selection != 3:
            raise ValueError("Month End unexpected selection count: " + str(self._month_end_selection))
        if self._month_start_selection != 3:
            raise ValueError("Month Start unexpected selection count: " + str(self._month_start_selection))
        if self._specific_date_selection != 1:
            raise ValueError("Specific date unexpected selection count: " + str(self._month_start_selection))
