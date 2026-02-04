# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-181E0A50
# Category: General Strategy
# Description: Strategy example algorithm using CAPE - a bubble indicator dataset saved in dropbox. CAPE is based on a macroeconomic...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Strategy example algorithm using CAPE - a bubble indicator dataset saved in dropbox. CAPE is based on a macroeconomic indicator(CAPE Ratio),
### we are looking for entry/exit points for momentum stocks CAPE data: January 1990 - December 2014
### Goals:
### Capitalize in overvalued markets by generating returns with momentum and selling before the crash
### Capitalize in undervalued markets by purchasing stocks at bottom of trough
### </summary>
### <meta name="tag" content="strategy example" />
### <meta name="tag" content="custom data" />
class BubbleAlgorithm(QCAlgorithm):

    def initialize(self):

        self.set_cash(100000)
        self.set_start_date(1998,1,1)
        self.set_end_date(2014,6,1)
        self._symbols = []
        self._macd_dic, self._rsi_dic = {},{}
        self._new_low, self._curr_cape = None, None
        self._counter, self._counter2 = 0, 0
        self._c, self._c_copy = np.empty([4]), np.empty([4])
        self._symbols.append("SPY")
        # add CAPE data
        self.add_data(Cape, "CAPE")
        
        # # Present Social Media Stocks:
        # self._symbols.append("FB"), self._symbols.append("LNKD"),self._symbols.append("GRPN"), self._symbols.append("TWTR")
        # self.set_start_date(2011, 1, 1)
        # self.set_end_date(2014, 12, 1)
        
        # # 2008 Financials
        # self._symbols.append("C"), self._symbols.append("AIG"), self._symbols.append("BAC"), self._symbols.append("HBOS")
        # self.set_start_date(2003, 1, 1)
        # self.set_end_date(2011, 1, 1)
        
        # # 2000 Dot.com
        # self._symbols.append("IPET"), self._symbols.append("WBVN"), self._symbols.append("GCTY")
        # self.set_start_date(1998, 1, 1)
        # self.set_end_date(2000, 1, 1)
        
        
        for stock in self._symbols:
            self.add_security(SecurityType.EQUITY, stock, Resolution.MINUTE)
            self._macd = self.macd(stock, 12, 26, 9, MovingAverageType.EXPONENTIAL, Resolution.DAILY)
            self._macd_dic[stock] = self._macd
            self._rsi = self.rsi(stock, 14, MovingAverageType.EXPONENTIAL, Resolution.DAILY)
            self._rsi_dic[stock] = self._rsi
        
    # Trying to find if current Cape is the lowest Cape in three months to indicate selling period
    def on_data(self, data):
        
        if self._curr_cape and self._new_low is not None:   
            try:
                # Bubble territory
                if self._curr_cape > 20 and self._new_low == False:
                    for stock in self._symbols:
                    # Order stock based on MACD
                    # During market hours, stock is trading, and sufficient cash
                        if self.securities[stock].holdings.quantity == 0 and self._rsi_dic[stock].current.value < 70 \
                        and self.securities[stock].price != 0 \
                        and self.portfolio.cash > self.securities[stock].price * 100 \
                        and self.time.hour == 9 and self.time.minute == 31:
                            self.buy_stock(stock)
                    # Utilize RSI for overbought territories and liquidate that stock
                        if self._rsi_dic[stock].current.value > 70 and self.securities[stock].holdings.quantity > 0 \
                        and self.time.hour == 9 and self.time.minute == 31:
                            self.sell_stock(stock)
                           
                # Undervalued territory            
                elif self._new_low:
                    for stock in self._symbols:
                        # Sell stock based on MACD
                        if self.securities[stock].holdings.quantity > 0 and self._rsi_dic[stock].current.value > 30 \
                        and self.time.hour == 9 and self.time.minute == 31:
                            self.sell_stock(stock)
                        # Utilize RSI and MACD to understand oversold territories
                        elif self.securities[stock].holdings.quantity == 0 and self._rsi_dic[stock].current.value < 30 \
                        and self.securities[stock].price != 0 and self.portfolio.cash > self.securities[stock].price * 100 \
                        and self.time.hour == 9 and self.time.minute == 31:
                            self.buy_stock(stock)
                
                # Cape Ratio is missing from original data
                # Most recent cape data is most likely to be missing 
                elif self._curr_cape == 0:
                    self.debug("Exiting due to no CAPE!")
                    self.quit("CAPE ratio not supplied in data, exiting.")
                
            except:
                # Do nothing
                return None       

        if not data.contains_key("CAPE"): return
        self._new_low = False
        # Adds first four Cape Ratios to array c
        self._curr_cape = data["CAPE"].cape
        if self._counter < 4:
            self._c[self._counter] = self._curr_cape
            self._counter +=1
        # Replaces oldest Cape with current Cape
        # Checks to see if current Cape is lowest in the previous quarter
        # Indicating a sell off
        else:
            self._c_copy = self._c  
            self._c_copy = np.sort(self._c_copy)
            if self._c_copy[0] > self._curr_cape:
                self._new_low = True
            self._c[self._counter2] = self._curr_cape
            self._counter2 += 1
            if self._counter2 == 4: self._counter2 = 0
        self.debug("Current Cape: " + str(self._curr_cape) + " on " + str(self.time))
        if self._new_low:
            self.debug("New Low has been hit on " + str(self.time))

    # Buy this symbol
    def buy_stock(self,symbol):
        s = self.securities[symbol].holdings
        if self._macd_dic[symbol].current.value>0:
            self.set_holdings(symbol, 1)
            self.debug("Purchasing: " + str(symbol) + "   MACD: " + str(self._macd_dic[symbol]) + "   RSI: " + str(self._rsi_dic[symbol])
                    + "   Price: " + str(round(self.securities[symbol].price, 2)) + "   Quantity: " + str(s.quantity))

    # Sell this symbol
    def sell_stock(self,symbol):
        s = self.securities[symbol].holdings
        if s.quantity > 0 and self._macd_dic[symbol].current.value < 0:
            self.liquidate(symbol)
            self.debug("Selling: " + str(symbol) + " at sell MACD: " + str(self._macd_dic[symbol]) + "   RSI: " + str(self._rsi_dic[symbol])
                    + "   Price: " + str(round(self.securities[symbol].price, 2)) + "   Profit from sale: " + str(s.last_trade_profit))


# CAPE Ratio for SP500 PE Ratio for avg inflation adjusted earnings for previous ten years Custom Data from DropBox
# Original Data from: http://www.econ.yale.edu/~shiller/data.htm
class Cape(PythonData):
    
    # Return the URL string source of the file. This will be converted to a stream
    # <param name="config">Configuration object</param>
    # <param name="date">Date of this source file</param>
    # <param name="is_live_mode">true if we're in live mode, false for backtesting mode</param>
    # <returns>String URL of source file.</returns>

    def get_source(self, config, date, is_live_mode):
        # Remember to add the "?dl=1" for dropbox links
        return SubscriptionDataSource("https://www.dropbox.com/s/ggt6blmib54q36e/CAPE.csv?dl=1", SubscriptionTransportMedium.REMOTE_FILE)
    
    
    ''' Reader Method : using set of arguments we specify read out type. Enumerate until 
        the end of the data stream or file. E.g. Read CSV file line by line and convert into data types. '''
        
    # <returns>BaseData type set by Subscription Method.</returns>
    # <param name="config">Config.</param>
    # <param name="line">Line.</param>
    # <param name="date">Date.</param>
    # <param name="is_live_mode">true if we're in live mode, false for backtesting mode</param>
    
    def reader(self, config, line, date, is_live_mode):
        if not (line.strip() and line[0].isdigit()): return None
    
        # New Nifty object
        index = Cape()
        index.symbol = config.symbol
    
        try:
            # Example File Format:
            # Date   |  Price |  Div  | Earning | CPI  | FractionalDate | Interest Rate | RealPrice | RealDiv | RealEarnings | CAPE
            # 2014.06  1947.09  37.38   103.12   238.343    2014.37          2.6           1923.95     36.94        101.89     25.55
            data = line.split(',')
            # Dates must be in the format YYYY-MM-DD. If your data source does not have this format, you must use
            # DateTime.parse_exact() and explicit declare the format your data source has.
            index.time = datetime.strptime(data[0], "%Y-%m")
            index["Cape"] = float(data[10]) 
            index.value = data[10]
            
    
        except ValueError:
                # Do nothing
                return None
    
        return index
