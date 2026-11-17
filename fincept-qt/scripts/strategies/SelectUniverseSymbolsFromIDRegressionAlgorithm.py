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
### Regression algorithm asserting that universe symbols selection can be done by returning the symbol IDs in the selection function
### </summary>
class SelectUniverseSymbolsFromIDRegressionAlgorithm(QCAlgorithm):
    '''
    Regression algorithm asserting that universe symbols selection can be done by returning the symbol IDs in the selection function
    '''

    def initialize(self):
        self.set_start_date(2014, 3, 24)
        self.set_end_date(2014, 3, 26)
        self.set_cash(100000)

        self._securities = []
        self.universe_settings.resolution = Resolution.DAILY
        self.add_universe(self.select_symbol)

    def select_symbol(self, fundamental):
        symbols = [x.symbol for x in fundamental]

        if not symbols:
            return []

        self.log(f"Symbols: {', '.join([str(s) for s in symbols])}")
        # Just for testing, but more filtering could be done here as shown below:
        #symbols = [x.symbol for x in fundamental if x.asset_classification.morningstar_sector_code == MorningstarSectorCode.TECHNOLOGY]

        history = self.history(symbols, datetime(1998, 1, 1), self.time, Resolution.DAILY)

        all_time_highs = history['high'].unstack(0).max()
        last_closes = history['close'].unstack(0).iloc[-1]
        security_ids = (last_closes / all_time_highs).sort_values().index[-5:]

        return security_ids

    def on_securities_changed(self, changes):
        self._securities.extend(changes.added_securities)

    def on_end_of_algorithm(self):
        if not self._securities:
            raise Exception("No securities were selected")
