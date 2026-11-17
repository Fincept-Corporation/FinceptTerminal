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
# Modified by Fincept Corporation for the Fincept Terminal: comment text changed; the code is unchanged.

from AlgorithmImports import *

### <summary>
### Universe Selection regression algorithm simulates an edge case. In one week, Google listed two new symbols, delisted one of them and changed tickers.
### </summary>
### <meta name="tag" content="regression test" />
class UniverseSelectionRegressionAlgorithm(QCAlgorithm):
    
    def initialize(self):
        
        self.set_start_date(2014,3,22)   #Set Start Date
        self.set_end_date(2014,4,7)      #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        # security that exists with no mappings
        self.add_equity("SPY", Resolution.DAILY)
        # security that doesn't exist until half way in backtest (comes in as GOOCV)
        self.add_equity("GOOG", Resolution.DAILY)

        self.universe_settings.resolution = Resolution.DAILY
        self.add_universe(self.coarse_selection_function)

        self.delisted_symbols = []
        self.changes = None


    def coarse_selection_function(self, coarse):
        return [ c.symbol for c in coarse if c.symbol.value == "GOOG" or c.symbol.value == "GOOCV" or c.symbol.value == "GOOAV" or c.symbol.value == "GOOGL" ]


    def on_data(self, data):
        if self.transactions.orders_count == 0:
            self.market_order("SPY", 100)

        for kvp in data.delistings:
            self.delisted_symbols.append(kvp.key)
        
        if self.changes is None:
            return

        if not all(data.bars.contains_key(x.symbol) for x in self.changes.added_securities):
            return 
        
        for security in self.changes.added_securities:
            self.log("{0}: Added Security: {1}".format(self.time, security.symbol))
            self.market_on_open_order(security.symbol, 100)

        for security in self.changes.removed_securities:
            self.log("{0}: Removed Security: {1}".format(self.time, security.symbol))
            if security.symbol not in self.delisted_symbols:
                self.log("Not in delisted: {0}:".format(security.symbol))
                self.market_on_open_order(security.symbol, -100)

        self.changes = None 


    def on_securities_changed(self, changes):
        self.changes = changes


    def on_order_event(self, orderEvent):
        if orderEvent.status == OrderStatus.SUBMITTED:
            self.log("{0}: Submitted: {1}".format(self.time, self.transactions.get_order_by_id(orderEvent.order_id)))
        if orderEvent.status == OrderStatus.FILLED:
            self.log("{0}: Filled: {1}".format(self.time, self.transactions.get_order_by_id(orderEvent.order_id)))
