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
### This algorithm demonstrate how to use Option Strategies (e.g. OptionStrategies.STRADDLE) helper classes to batch send orders for common strategies.
### It also shows how you can prefilter contracts easily based on strikes and expirations, and how you can inspect the
### option chain to pick a specific option contract to trade.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="options" />
### <meta name="tag" content="option strategies" />
### <meta name="tag" content="filter selection" />
class BasicTemplateOptionStrategyAlgorithm(QCAlgorithm):

    def initialize(self):
        # Set the cash we'd like to use for our backtest
        self.set_cash(1000000)

        # Start and end dates for the backtest.
        self.set_start_date(2015,12,24)
        self.set_end_date(2015,12,24)

        # Add assets you'd like to see
        option = self.add_option("GOOG")
        self.option_symbol = option.symbol

        # set our strike/expiry filter for this option chain
        # SetFilter method accepts timedelta objects or integer for days.
        # The following statements yield the same filtering criteria
        option.set_filter(-2, +2, 0, 180)
        # option.set_filter(-2,2, timedelta(0), timedelta(180))

        # use the underlying equity as the benchmark
        self.set_benchmark("GOOG")

    def on_data(self,slice):
        if not self.portfolio.invested:
            for kvp in slice.option_chains:
                chain = kvp.value
                contracts = sorted(sorted(chain, key = lambda x: abs(chain.underlying.price - x.strike)),
                                                 key = lambda x: x.expiry, reverse=False)

                if len(contracts) == 0: continue
                atm_straddle = contracts[0]
                if atm_straddle != None:
                    self.sell(OptionStrategies.STRADDLE(self.option_symbol, atm_straddle.strike, atm_straddle.expiry), 2)
        else:
            self.liquidate()

    def on_order_event(self, order_event):
        self.log(str(order_event))
