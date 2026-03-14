# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C02B7DE9
# Category: Universe Selection
# Description: In this algorithm we show how you can easily use the universe selection feature to fetch symbols to be traded using t...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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
