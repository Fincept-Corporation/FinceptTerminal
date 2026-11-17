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
class BasicTemplateOptionsDailyAlgorithm(QCAlgorithm):
    underlying_ticker = "AAPL"

    def initialize(self):
        self.set_start_date(2015, 12, 15)
        self.set_end_date(2016, 2, 1)
        self.set_cash(100000)
        self.option_expired = False

        equity = self.add_equity(self.underlying_ticker, Resolution.DAILY)
        option = self.add_option(self.underlying_ticker, Resolution.DAILY)
        self.option_symbol = option.symbol

        # set our strike/expiry filter for this option chain
        option.set_filter(lambda u: (u.calls_only().expiration(0, 60)))

        # use the underlying equity as the benchmark
        self.set_benchmark(equity.symbol)

    def on_data(self,slice):
        if self.portfolio.invested: return

        chain = slice.option_chains.get_value(self.option_symbol)
        if chain is None:
            return

        # Grab us the contract nearest expiry
        contracts = sorted(chain, key = lambda x: x.expiry)

        # if found, trade it
        if len(contracts) == 0: return
        symbol = contracts[0].symbol
        self.market_order(symbol, 1)

    def on_order_event(self, order_event):
        self.log(str(order_event))

        # Check for our expected OTM option expiry
        if "OTM" in order_event.message:

            # Assert it is at midnight 1/16 (5AM UTC)
            if order_event.utc_time.month != 1 and order_event.utc_time.day != 16 and order_event.utc_time.hour != 5:
                raise AssertionError(f"Expiry event was not at the correct time, {order_event.utc_time}")

            self.option_expired = True

    def on_end_of_algorithm(self):
        # Assert we had our option expire and fill a liquidation order
        if not self.option_expired:
            raise AssertionError("Algorithm did not process the option expiration like expected")
