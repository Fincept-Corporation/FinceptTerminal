# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-43A9D681
# Category: General Strategy
# Description: In this algorithm, we fetch a list of tickers with corresponding dates from a file on Dropbox. We then create a fine ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### In this algorithm, we fetch a list of tickers with corresponding dates from a file on Dropbox.
### We then create a fine fundamental universe which contains those symbols on their respective dates.### 
### </summary>
### <meta name="tag" content="download" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="custom data" />
class DropboxCoarseFineAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2019, 9, 23) # Set Start Date
        self.set_end_date(2019, 9, 30) # Set End Date
        self.set_cash(100000)  # Set Strategy Cash
        self.add_universe(self.select_coarse, self.select_fine)
       
        self.universe_data = None
        self.next_update = datetime(1, 1, 1) # Minimum datetime 
        self.url = "https://www.dropbox.com/s/x2sb9gaiicc6hm3/tickers_with_dates.csv?dl=1" 
        
    def on_end_of_day(self): 
        for security in self.active_securities.values:
            self.debug(f"{self.time.date()} {security.symbol.value} with Market Cap: ${security.fundamentals.market_cap}")
        
    def select_coarse(self, coarse):
        return self.get_symbols()
        
    def select_fine(self, fine): 
        symbols = self.get_symbols()
        
        # Return symbols from our list which have a market capitalization of at least 10B
        return [f.symbol for f in fine if f.market_cap > 1e10 and f.symbol in symbols]
        
    def get_symbols(self):
        
        # In live trading update every 12 hours
        if self.live_mode:
            if self.time < self.next_update:
                # Return today's row
                return self.universe_data[self.time.date()]
            # When updating set the new reset time.
            self.next_update = self.time + timedelta(hours=12)
            self.universe_data = self.parse(self.url) 
        
        # In backtest load once if not set, then just use the dates.
        if self.universe_data is None:
            self.universe_data = self.parse(self.url)

        # Check if contains the row we need
        if self.time.date() not in self.universe_data:
            return Universe.unchanged

        return self.universe_data[self.time.date()]
        
    
    def parse(self, url): 
        # Download file from url as string
        file = self.download(url).split("\n")
        
        # # Remove formatting characters
        data = [x.replace("\r", "").replace(" ", "") for x in file]
        
        # # Split data by date and symbol
        split_data = [x.split(",") for x in data]
        
        # Dictionary to hold list of active symbols for each date, keyed by date
        symbols_by_date = {}
        
        # Parse data into dictionary
        for arr in split_data:
            date = datetime.strptime(arr[0], "%Y%m%d").date()
            symbols = [Symbol.create(ticker, SecurityType.EQUITY, Market.USA) for ticker in arr[1:]]
            symbols_by_date[date] = symbols
        
        return symbols_by_date
        
    def on_securities_changed(self, changes):
        self.log(f"Added Securities: {[security.symbol.value for security in changes.added_securities]}")
