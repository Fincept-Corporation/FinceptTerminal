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

class HistoryRequestBenchmark(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2010, 1, 1)
        self.set_end_date(2018, 1, 1)
        self.set_cash(10000)
        self._symbol = self.add_equity("SPY").symbol

    def on_end_of_day(self, symbol):
        minute_history = self.history([self._symbol], 60, Resolution.MINUTE)
        last_hour_high = 0
        for index, row in minute_history.loc["SPY"].iterrows():
            if last_hour_high < row["high"]:
                last_hour_high = row["high"]

        daily_history = self.history([self._symbol], 1, Resolution.DAILY).loc["SPY"].head()
        daily_history_high = daily_history["high"]
        daily_history_low = daily_history["low"]
        daily_history_open = daily_history["open"]
