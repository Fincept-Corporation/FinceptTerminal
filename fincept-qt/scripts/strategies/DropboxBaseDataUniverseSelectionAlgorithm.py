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
from System.Collections.Generic import List

### <summary>
### In this algorithm we show how you can easily use the universe selection feature to fetch symbols
### to be traded using the BaseData custom data system in combination with the AddUniverse{T} method.
### AddUniverse{T} requires a function that will return the symbols to be traded.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="custom universes" />
class DropboxBaseDataUniverseSelectionAlgorithm(QCAlgorithm):

    def initialize(self):

        self.universe_settings.resolution = Resolution.DAILY

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

        self.set_start_date(2017, 7, 6)
        self.set_end_date(2018, 7, 4)

        universe = self.add_universe(StockDataSource, self.stock_data_source)

        historical_selection_data = self.history(universe, 3)
        if len(historical_selection_data) != 3:
            raise ValueError(f"Unexpected universe data count {len(historical_selection_data)}")

        for universe_data in historical_selection_data["symbols"]:
            if len(universe_data) != 5:
                raise ValueError(f"Unexpected universe data receieved")

    def stock_data_source(self, data):
        list = []
        for item in data:
            for symbol in item["Symbols"]:
                list.append(symbol)
        return list

    def on_data(self, slice):

        if slice.bars.count == 0: return
        if self._changes is None: return

        # start fresh
        self.liquidate()

        percentage = 1 / slice.bars.count
        for trade_bar in slice.bars.values():
            self.set_holdings(trade_bar.symbol, percentage)

        # reset changes
        self._changes = None

    def on_securities_changed(self, changes):
        self._changes = changes

class StockDataSource(PythonData):

    def get_source(self, config, date, is_live_mode):
        url = "https://www.dropbox.com/s/2l73mu97gcehmh7/daily-stock-picker-live.csv?dl=1" if is_live_mode else \
            "https://www.dropbox.com/s/ae1couew5ir3z9y/daily-stock-picker-backtest.csv?dl=1"

        return SubscriptionDataSource(url, SubscriptionTransportMedium.REMOTE_FILE)

    def reader(self, config, line, date, is_live_mode):
        if not (line.strip() and line[0].isdigit()): return None

        stocks = StockDataSource()
        stocks.symbol = config.symbol

        csv = line.split(',')
        if is_live_mode:
            stocks.time = date
            stocks["Symbols"] = csv
        else:
            stocks.time = datetime.strptime(csv[0], "%Y%m%d")
            stocks["Symbols"] = csv[1:]
        return stocks
