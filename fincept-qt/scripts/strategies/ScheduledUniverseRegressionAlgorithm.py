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
### Regression algorithm asserting the behavior of a ScheduledUniverse
### </summary>
class BasicTemplateAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10, 8)
        
        self._spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        self._selection_time =[ datetime(2013, 10, 7, 1, 0, 0), datetime(2013, 10, 8, 1, 0, 0)]

        self.add_universe(ScheduledUniverse(self.date_rules.every_day(), self.time_rules.at(1, 0), self.select_assets))


    def select_assets(self, time):
        self.debug(f"Universe selection called: {Time}")
        expected_time = self._selection_time.pop(0)
        if expected_time != self.time:
            raise ValueError(f"Unexpected selection time {self.time} expected {expected_time}")

        return [ self._spy ]
    
    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings(self._spy, 1)

    def on_end_of_algorithm(self):
        if len(self._selection_time) > 0:
            raise ValueError("Unexpected selection times")
