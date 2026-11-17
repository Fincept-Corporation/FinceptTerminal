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
### This example demonstrates how to add options for a given underlying equity security.
### It also shows how you can prefilter contracts easily based on strikes and expirations, and how you
### can inspect the option chain to pick a specific option contract to trade.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="options" />
### <meta name="tag" content="filter selection" />
class BasicTemplateOptionsHourlyAlgorithm(QCAlgorithm):
    underlying_ticker = "AAPL"

    def initialize(self):
        self.set_start_date(2014, 6, 6)
        self.set_end_date(2014, 6, 9)
        self.set_cash(100000)

        equity = self.add_equity(self.underlying_ticker, Resolution.HOUR)
        option = self.add_option(self.underlying_ticker, Resolution.HOUR)
        self.option_symbol = option.symbol

        # set our strike/expiry filter for this option chain
        option.set_filter(lambda u: (u.strikes(-2, +2)
                                     # Expiration method accepts TimeSpan objects or integer for days.
                                     # The following statements yield the same filtering criteria
                                     .expiration(0, 180)))
                                     #.expiration(TimeSpan.zero, TimeSpan.from_days(180))))

        # use the underlying equity as the benchmark
        self.set_benchmark(equity.symbol)

    def on_data(self,slice):
        if self.portfolio.invested or not self.is_market_open(self.option_symbol): return

        chain = slice.option_chains.get_value(self.option_symbol)
        if chain is None:
            return

        # we sort the contracts to find at the money (ATM) contract with farthest expiration
        contracts = sorted(sorted(sorted(chain, \
            key = lambda x: abs(chain.underlying.price - x.strike)), \
            key = lambda x: x.expiry, reverse=True), \
            key = lambda x: x.right, reverse=True)

        # if found, trade it
        if len(contracts) == 0 or not self.is_market_open(contracts[0].symbol): return
        symbol = contracts[0].symbol
        self.market_order(symbol, 1)
        self.market_on_close_order(symbol, -1)

    def on_order_event(self, order_event):
        self.log(str(order_event))
