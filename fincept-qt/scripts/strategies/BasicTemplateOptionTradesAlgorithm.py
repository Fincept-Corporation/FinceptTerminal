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
### It also shows how you can prefilter contracts easily based on strikes and expirations.
### It also shows how you can inspect the option chain to pick a specific option contract to trade.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="options" />
### <meta name="tag" content="filter selection" />
class BasicTemplateOptionTradesAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(100000)

        option = self.add_option("GOOG")

        # add the initial contract filter 
        # SetFilter method accepts timedelta objects or integer for days.
        # The following statements yield the same filtering criteria
        option.set_filter(-2, +2, 0, 10)
        # option.set_filter(-2, +2, timedelta(0), timedelta(10))

        # use the underlying equity as the benchmark
        self.set_benchmark("GOOG")

    def on_data(self,slice):
        if not self.portfolio.invested:
            for kvp in slice.option_chains:
                chain = kvp.value
                # find the second call strike under market price expiring today
                contracts = sorted(sorted(chain, key = lambda x: abs(chain.underlying.price - x.strike)),
                                                 key = lambda x: x.expiry, reverse=False)

                if len(contracts) == 0: continue
                if contracts[0] != None:
                    self.market_order(contracts[0].symbol, 1)
        else:
            self.liquidate()

        for kpv in slice.bars:
            self.log("---> OnData: {0}, {1}, {2}".format(self.time, kpv.key.value, str(kpv.value.close)))

    def on_order_event(self, order_event):
        self.log(str(order_event))
