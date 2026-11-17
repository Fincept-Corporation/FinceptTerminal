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
### This example demonstrates how to add and trade SPX index weekly options
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="options" />
### <meta name="tag" content="indexes" />
class BasicTemplateSPXWeeklyIndexOptionsAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 1, 10)
        self.set_cash(1000000)

        # regular option SPX contracts
        self.spx_options = self.add_index_option("SPX")
        self.spx_options.set_filter(lambda u: (u.strikes(0, 1).expiration(0, 30)))

        # weekly option SPX contracts
        spxw = self.add_index_option("SPX", "SPXW")
        # set our strike/expiry filter for this option chain
        spxw.set_filter(lambda u: (u.strikes(0, 1)
                                     # single week ahead since there are many SPXW contracts and we want to preserve performance
                                     .expiration(0, 7)
                                     .include_weeklys()))

        self.spxw_option = spxw.symbol

    def on_data(self,slice):
        if self.portfolio.invested: return

        chain = slice.option_chains.get_value(self.spxw_option)
        if chain is None:
            return

        # we sort the contracts to find at the money (ATM) contract with closest expiration
        contracts = sorted(sorted(sorted(chain, \
            key = lambda x: x.expiry), \
            key = lambda x: abs(chain.underlying.price - x.strike)), \
            key = lambda x: x.right, reverse=True)

        # if found, buy until it expires
        if len(contracts) == 0: return
        symbol = contracts[0].symbol
        self.market_order(symbol, 1)

    def on_order_event(self, order_event):
        self.debug(str(order_event))
