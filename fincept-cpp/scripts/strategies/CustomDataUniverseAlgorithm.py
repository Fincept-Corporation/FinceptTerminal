# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A032E1EE
# Category: Universe Selection
# Description: This algorithm shows how to grab symbols from an external api each day and load data using the universe selection fea...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This algorithm shows how to grab symbols from an external api each day
### and load data using the universe selection feature. In this example we
### define a custom data type for the NYSE top gainers and then short the
### top 2 gainers each day
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="custom universes" />
class CustomDataUniverseAlgorithm(QCAlgorithm):

    def initialize(self):

        # Data ADDED via universe selection is added with Daily resolution.
        self.universe_settings.resolution = Resolution.DAILY

        self.set_start_date(2015,1,5)
        self.set_end_date(2015,7,1)
        self.set_cash(100000)

        self.add_equity("SPY", Resolution.DAILY)
        self.set_benchmark("SPY")

        # add a custom universe data source (defaults to usa-equity)
        self.add_universe(NyseTopGainers, "universe-nyse-top-gainers", Resolution.DAILY, self.nyse_top_gainers)
    
    def nyse_top_gainers(self, data):
        return [ x.symbol for x in data if x["TopGainersRank"] <= 2 ]


    def on_data(self, slice):
        pass
    
    def on_securities_changed(self, changes):
        self._changes = changes

        for security in changes.removed_securities:
            #  liquidate securities that have been removed
            if security.invested:
                self.liquidate(security.symbol)
                self.log("Exit {0} at {1}".format(security.symbol, security.close))

        for security in changes.added_securities:
            # enter short positions on new securities
            if not security.invested and security.close != 0:
                qty = self.calculate_order_quantity(security.symbol, -0.25)
                self.market_on_open_order(security.symbol, qty)
                self.log("Enter {0} at {1}".format(security.symbol, security.close))

        
class NyseTopGainers(PythonData):
    def __init__(self):
        self.count = 0
        self.last_date = datetime.min

    def get_source(self, config, date, is_live_mode):
        url = "http://www.wsj.com/mdc/public/page/2_3021-gainnyse-gainer.html" if is_live_mode else \
            "https://www.dropbox.com/s/vrn3p38qberw3df/nyse-gainers.csv?dl=1"

        return SubscriptionDataSource(url, SubscriptionTransportMedium.REMOTE_FILE)
    
    def reader(self, config, line, date, is_live_mode):
        
        if not is_live_mode:
            # backtest gets data from csv file in dropbox
            if not (line.strip() and line[0].isdigit()): return None
            csv = line.split(',')
            nyse = NyseTopGainers()
            nyse.time = datetime.strptime(csv[0], "%Y%m%d")
            nyse.end_time = nyse.time + timedelta(1)
            nyse.symbol = Symbol.create(csv[1], SecurityType.EQUITY, Market.USA)
            nyse["TopGainersRank"] = int(csv[2])
            return nyse

        if self.last_date != date:
            # reset our counter for the new day
            self.last_date = date
            self.count = 0
        
        # parse the html into a symbol
        if not line.startswith('<a href=\"/public/quotes/main.html?symbol='):
            # we're only looking for lines that contain the symbols
            return None
        
        last_close_paren = line.rfind(')')
        last_open_paren = line.rfind('(')
        if last_open_paren == -1 or last_close_paren == -1:
            return None

        symbol_string = line[last_open_paren + 1:last_close_paren]
        nyse = NyseTopGainers()
        nyse.time = date
        nyse.end_time = nyse.time + timedelta(1)
        nyse.symbol = Symbol.create(symbol_string, SecurityType.EQUITY, Market.USA)
        nyse["TopGainersRank"] = self.count
        self.count = self.count + 1
        return nyse
