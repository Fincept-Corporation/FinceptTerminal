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
### Regression algorithm to test the OptionChainedUniverseSelectionModel class
### </summary>
class OptionChainedUniverseSelectionModelRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.universe_settings.resolution = Resolution.MINUTE
        self.set_start_date(2014, 6, 6)
        self.set_end_date(2014, 6, 6)
        self.set_cash(100000)

        universe = self.add_universe("my-minute-universe-name", lambda time: [ "AAPL", "TWX" ])
        self.add_universe_selection(
            OptionChainedUniverseSelectionModel(
                universe,
                lambda u: (u.strikes(-2, +2)
                                     # Expiration method accepts TimeSpan objects or integer for days.
                                     # The following statements yield the same filtering criteria
                                     .expiration(0, 180))
            )
        )

    def on_data(self, slice):
        if self.portfolio.invested or not (self.is_market_open("AAPL") and self.is_market_open("TWX")): return
        values = list(map(lambda x: x.value, filter(lambda x: x.key == "?AAPL" or x.key == "?TWX",  slice.option_chains)))
        for chain in values:
            # we sort the contracts to find at the money (ATM) contract with farthest expiration
            contracts = sorted(sorted(sorted(chain, \
                key = lambda x: abs(chain.underlying.price - x.strike)), \
                key = lambda x: x.expiry, reverse=True), \
                key = lambda x: x.right, reverse=True)

            # if found, trade it
            if len(contracts) == 0: return
            symbol = contracts[0].symbol
            self.market_order(symbol, 1)
            self.market_on_close_order(symbol, -1)
