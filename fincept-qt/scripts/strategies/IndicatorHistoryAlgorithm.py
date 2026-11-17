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
### Demonstration algorithm of indicators history window usage
### </summary>
class IndicatorHistoryAlgorithm(QCAlgorithm):
    '''Demonstration algorithm of indicators history window usage.'''

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2013, 1, 1)
        self.set_end_date(2014, 12, 31)
        self.set_cash(25000)

        self._symbol = self.add_equity("SPY", Resolution.DAILY).symbol

        self.bollinger_bands = self.bb(self._symbol, 20, 2.0, resolution=Resolution.DAILY)
        # Let's keep BB values for a 20 day period
        self.bollinger_bands.window.size = 20
        # Also keep the same period of data for the middle band
        self.bollinger_bands.middle_band.window.size = 20;

    def on_data(self, slice: Slice):
        # Let's wait for our indicator to fully initialize and have a full window of history data
        if not self.bollinger_bands.window.is_ready: return

        # We can access the current and oldest (in our period) values of the indicator
        self.log(f"Current BB value: {self.bollinger_bands[0].end_time} - {self.bollinger_bands[0].value}")
        self.log(f"Oldest BB value: {self.bollinger_bands[self.bollinger_bands.window.count - 1].end_time} - "
                 f"{self.bollinger_bands[self.bollinger_bands.window.count - 1].value}")

        # Let's log the BB values for the last 20 days, for demonstration purposes on how it can be enumerated
        for data_point in self.bollinger_bands:
            self.log(f"BB @{data_point.end_time}: {data_point.value}")

        # We can also do the same for internal indicators:
        middle_band = self.bollinger_bands.middle_band
        self.log(f"Current BB Middle Band value: {middle_band[0].end_time} - {middle_band[0].value}")
        self.log(f"Oldest BB Middle Band value: {middle_band[middle_band.window.count - 1].end_time} - "
                 f"{middle_band[middle_band.window.count - 1].value}")
        for data_point in middle_band:
            self.log(f"BB Middle Band @{data_point.end_time}: {data_point.value}")

        # We are done now!
        self.quit()
