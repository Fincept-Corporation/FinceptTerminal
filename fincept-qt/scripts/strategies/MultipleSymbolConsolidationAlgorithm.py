# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-CFDE4C40
# Category: Data Consolidation
# Description: Example structure for structuring an algorithm with indicator and consolidator data for many tickers
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example structure for structuring an algorithm with indicator and consolidator data for many tickers.
### </summary>
### <meta name="tag" content="consolidating data" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="using data" />
### <meta name="tag" content="strategy example" />
class MultipleSymbolConsolidationAlgorithm(QCAlgorithm):
    
    # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    def initialize(self):
        
        # This is the period of bars we'll be creating
        bar_period = TimeSpan.from_minutes(10)
        # This is the period of our sma indicators
        sma_period = 10
        # This is the number of consolidated bars we'll hold in symbol data for reference
        rolling_window_size = 10
        # Holds all of our data keyed by each symbol
        self.data = {}
        # Contains all of our equity symbols
        equity_symbols = ["AAPL","SPY","IBM"]
        # Contains all of our forex symbols
        forex_symbols = ["EURUSD", "USDJPY", "EURGBP", "EURCHF", "USDCAD", "USDCHF", "AUDUSD","NZDUSD"]
        
        self.set_start_date(2014, 12, 1)
        self.set_end_date(2015, 2, 1)
        
        # initialize our equity data
        for symbol in equity_symbols:
            equity = self.add_equity(symbol)
            self.data[symbol] = SymbolData(equity.symbol, bar_period, rolling_window_size)
        
        # initialize our forex data 
        for symbol in forex_symbols:
            forex = self.add_forex(symbol)
            self.data[symbol] = SymbolData(forex.symbol, bar_period, rolling_window_size)

        # loop through all our symbols and request data subscriptions and initialize indicator
        for symbol, symbol_data in self.data.items():
            # define the indicator
            symbol_data.sma = SimpleMovingAverage(self.create_indicator_name(symbol, "sma" + str(sma_period), Resolution.MINUTE), sma_period)
            # define a consolidator to consolidate data for this symbol on the requested period
            consolidator = TradeBarConsolidator(bar_period) if symbol_data.symbol.security_type == SecurityType.EQUITY else QuoteBarConsolidator(bar_period)
            # write up our consolidator to update the indicator
            consolidator.data_consolidated += self.on_data_consolidated
            # we need to add this consolidator so it gets auto updates
            self.subscription_manager.add_consolidator(symbol_data.symbol, consolidator)

    def on_data_consolidated(self, sender, bar):
        
        self.data[bar.symbol.value].sma.update(bar.time, bar.close)
        self.data[bar.symbol.value].bars.add(bar)

    # OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    # Argument "data": Slice object, dictionary object with your stock data 
    def on_data(self,data):
        
        # loop through each symbol in our structure
        for symbol in self.data.keys():
            symbol_data = self.data[symbol]
            # this check proves that this symbol was JUST updated prior to this OnData function being called
            if symbol_data.is_ready() and symbol_data.was_just_updated(self.time):
                if not self.portfolio[symbol].invested:
                    self.market_order(symbol, 1)

    # End of a trading day event handler. This method is called at the end of the algorithm day (or multiple times if trading multiple assets).
    # Method is called 10 minutes before closing to allow user to close out position.
    def on_end_of_day(self, symbol):
        
        i = 0
        for symbol in sorted(self.data.keys()):
            symbol_data = self.data[symbol]
            # we have too many symbols to plot them all, so plot every other
            i += 1
            if symbol_data.is_ready() and i%2 == 0:
                self.plot(symbol, symbol, symbol_data.sma.current.value)
    
       
class SymbolData(object):
    
    def __init__(self, symbol, bar_period, window_size):
        self._symbol = symbol
        self.symbol = symbol
        # The period used when population the Bars rolling window
        self.bar_period = bar_period
        # A rolling window of data, data needs to be pumped into Bars by using Bars.update( trade_bar ) and can be accessed like:
        # my_symbol_data.bars[0] - most first recent piece of data
        # my_symbol_data.bars[5] - the sixth most recent piece of data (zero based indexing)
        self.bars = RollingWindow[IBaseDataBar](window_size)
        # The simple moving average indicator for our symbol
        self.sma = None
  
    # Returns true if all the data in this instance is ready (indicators, rolling windows, ect...)
    def is_ready(self):
        return self.bars.is_ready and self.sma.is_ready

    # Returns true if the most recent trade bar time matches the current time minus the bar's period, this
    # indicates that update was just called on this instance
    def was_just_updated(self, current):
        return self.bars.count > 0 and self.bars[0].time == current - self.bar_period                                               
