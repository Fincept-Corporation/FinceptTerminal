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
### Example algorithm of how to use RangeConsolidator
### </summary>
class RangeConsolidatorAlgorithm(QCAlgorithm):
    def get_resolution(self):
        return Resolution.DAILY

    def get_range(self):
        return 100

    def initialize(self):        
        self.set_start_and_end_dates();
        self.add_equity("SPY", self.get_resolution())
        range_consolidator = self.create_range_consolidator()
        range_consolidator.data_consolidated += self.on_data_consolidated
        self.first_data_consolidated = None;

        self.subscription_manager.add_consolidator("SPY", range_consolidator)

    def set_start_and_end_dates(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

    def on_end_of_algorithm(self):
        if self.first_data_consolidated == None:
            raise Exception("The consolidator should have consolidated at least one RangeBar, but it did not consolidated any one")

    def create_range_consolidator(self):
        return RangeConsolidator(self.get_range())

    def on_data_consolidated(self, sender, range_bar):
        if (self.first_data_consolidated is None):
            self.first_data_consolidated = range_bar

        if round(range_bar.high - range_bar.low, 2) != self.get_range() * 0.01: # The minimum price change for SPY is 0.01, therefore the range size of each bar equals Range * 0.01
            raise Exception(f"The difference between the High and Low for all RangeBar's should be {self.get_range() * 0.01}, but for this RangeBar was {round(range_bar.low - range_bar.high, 2)}")
