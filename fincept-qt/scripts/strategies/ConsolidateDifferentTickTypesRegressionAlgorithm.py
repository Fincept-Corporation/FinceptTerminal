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
### This algorithm asserts we can consolidate Tick data with different tick types
### </summary>
class ConsolidateDifferentTickTypesRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 6)
        self.set_end_date(2013, 10, 7)
        self.set_cash(1000000)

        equity = self.add_equity("SPY", Resolution.TICK, Market.USA)
        quote_consolidator = self.consolidate(equity.symbol, Resolution.TICK, TickType.QUOTE, lambda tick : self.on_quote_tick(tick))
        self.there_is_at_least_one_quote_tick = False

        trade_consolidator = self.consolidate(equity.symbol, Resolution.TICK, TickType.TRADE, lambda tick : self.on_trade_tick(tick))
        self.there_is_at_least_one_trade_tick = False

    def on_quote_tick(self, tick):
        self.there_is_at_least_one_quote_tick = True
        if tick.tick_type != TickType.QUOTE:
            raise Exception(f"The type of the tick should be Quote, but was {tick.tick_type}")

    def on_trade_tick(self, tick):
        self.there_is_at_least_one_trade_tick = True
        if tick.tick_type != TickType.TRADE:
            raise Exception(f"The type of the tick should be Trade, but was {tick.tick_type}")

    def on_end_of_algorithm(self):
        if not self.there_is_at_least_one_quote_tick:
            raise Exception(f"There should have been at least one tick in OnQuoteTick() method, but there wasn't")

        if not self.there_is_at_least_one_trade_tick:
            raise Exception(f"There should have been at least one tick in OnTradeTick() method, but there wasn't")

