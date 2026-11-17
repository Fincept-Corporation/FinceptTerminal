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
### Regression algorithm asserting the behavior of the indicator history api
### </summary>
class IndicatorHistoryRegressionAlgorithm(QCAlgorithm):
    '''Regression algorithm asserting the behavior of the indicator history api'''

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2013, 1, 1)
        self.set_end_date(2014, 12, 31)

        self._symbol = self.add_equity("SPY", Resolution.DAILY).symbol

    def on_data(self, slice: Slice):
        self.bollinger_bands = BollingerBands("BB", 20, 2.0, MovingAverageType.SIMPLE)

        if self.bollinger_bands.window.is_ready:
            raise ValueError("Unexpected ready bollinger bands state")

        indicatorHistory = self.indicator_history(self.bollinger_bands, self._symbol, 50)
        
        self.debug(f"indicatorHistory: {indicatorHistory}")

        self.debug(f"data_frame: {indicatorHistory.data_frame}")

        if not self.bollinger_bands.window.is_ready:
            raise ValueError("Unexpected not ready bollinger bands state")

        # we ask for 50 data points
        if indicatorHistory.count != 50:
            raise ValueError(f"Unexpected indicators values {indicatorHistory.count}")

        for indicatorDataPoints in indicatorHistory:
            middle_band = indicatorDataPoints["middle_band"]
            self.debug(f"BB @{indicatorDataPoints.current}: middle_band: {middle_band} upper_band: {indicatorDataPoints.upper_band}")
            if indicatorDataPoints == 0:
                raise ValueError(f"Unexpected indicators point {indicatorDataPoints}")

        currentValues = indicatorHistory.current
        if len(currentValues) != 50 or len([x for x in currentValues if x.value == 0]) > 0:
            raise ValueError(f"Unexpected indicators current values {len(currentValues)}")
        upperBandPoints = indicatorHistory["upper_band"]
        if len(upperBandPoints) != 50 or len([x for x in upperBandPoints if x.value == 0]) > 0:
            raise ValueError(f"Unexpected indicators upperBandPoints values {len(upperBandPoints)}")

        # We are done now!
        self.quit()
