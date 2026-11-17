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
### A demonstration of consolidating options data into larger bars for your algorithm.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="benchmarks" />
### <meta name="tag" content="consolidating data" />
### <meta name="tag" content="options" />
class BasicTemplateOptionsConsolidationAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(1000000)

        # Subscribe and set our filter for the options chain
        option = self.add_option('SPY')
        # set our strike/expiry filter for this option chain
        # SetFilter method accepts timedelta objects or integer for days.
        # The following statements yield the same filtering criteria
        option.set_filter(-2, +2, 0, 180)
        # option.set_filter(-2, +2, timedelta(0), timedelta(180))
        self.consolidators = dict()

    def on_quote_bar_consolidated(self, sender, quote_bar):
        self.log("OnQuoteBarConsolidated called on " + str(self.time))
        self.log(str(quote_bar))

    def on_trade_bar_consolidated(self, sender, trade_bar):
        self.log("OnTradeBarConsolidated called on " + str(self.time))
        self.log(str(trade_bar))
        
    def on_securities_changed(self, changes):
        for security in changes.added_securities:
            if security.type == SecurityType.EQUITY:
                consolidator = TradeBarConsolidator(timedelta(minutes=5))
                consolidator.data_consolidated += self.on_trade_bar_consolidated
            else:
                consolidator = QuoteBarConsolidator(timedelta(minutes=5))
                consolidator.data_consolidated += self.on_quote_bar_consolidated
                
            self.subscription_manager.add_consolidator(security.symbol, consolidator)
            self.consolidators[security.symbol] = consolidator
            
        for security in changes.removed_securities:
            consolidator = self.consolidators.pop(security.symbol)
            self.subscription_manager.remove_consolidator(security.symbol, consolidator)
            
            if security.type == SecurityType.EQUITY:
                consolidator.data_consolidated -= self.on_trade_bar_consolidated
            else:
                consolidator.data_consolidated -= self.on_quote_bar_consolidated
