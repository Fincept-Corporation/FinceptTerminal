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
import base64

### <summary>
### In this algorithm we show how you can easily use the universe selection feature to fetch symbols
### to be traded using the BaseData custom data system in combination with the AddUniverse{T} method.
### AddUniverse{T} requires a function that will return the symbols to be traded.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="custom universes" />
class DropboxUniverseSelectionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2017, 7, 4)
        self.set_end_date(2018, 7, 4)

        self.backtest_symbols_per_day = {}
        self.current_universe = []

        self.universe_settings.resolution = Resolution.DAILY

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

        self.add_universe("my-dropbox-universe", self.selector)

    def selector(self, date):
        # handle live mode file format
        if self.live_mode:
            # fetch the file from dropbox
            str = self.download("https://www.dropbox.com/s/2l73mu97gcehmh7/daily-stock-picker-live.csv?dl=1")
            # if we have a file for today, return symbols, else leave universe unchanged
            self.current_universe = str.split(',') if len(str) > 0 else self.current_universe
            return self.current_universe

        # backtest - first cache the entire file
        if len(self.backtest_symbols_per_day) == 0:

            # No need for headers for authorization with dropbox, these two lines are for example purposes 
            byte_key = base64.b64encode("UserName:Password".encode('ASCII'))
            # The headers must be passed to the Download method as dictionary
            headers = { 'Authorization' : f'Basic ({byte_key.decode("ASCII")})' }

            str = self.download("https://www.dropbox.com/s/ae1couew5ir3z9y/daily-stock-picker-backtest.csv?dl=1", headers)
            for line in str.splitlines():
                data = line.split(',')
                self.backtest_symbols_per_day[data[0]] = data[1:]

        index = date.strftime("%Y%m%d")
        self.current_universe = self.backtest_symbols_per_day.get(index, self.current_universe)

        return self.current_universe

    def on_data(self, slice):

        if slice.bars.count == 0: return
        if self.changes is None: return

        # start fresh
        self.liquidate()

        percentage = 1 / slice.bars.count
        for trade_bar in slice.bars.values():
            self.set_holdings(trade_bar.symbol, percentage)

        # reset changes
        self.changes = None

    def on_securities_changed(self, changes):
        self.changes = changes
