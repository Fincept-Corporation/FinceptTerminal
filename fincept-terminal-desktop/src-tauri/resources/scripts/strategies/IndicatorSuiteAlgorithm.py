# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-71C08CCA
# Category: Indicators
# Description: Basic template algorithm simply initializes the date range and cash. This is a skeleton framework you can use for des...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from QuantConnect.Algorithm.CSharp import *

### <summary>
### Basic template algorithm simply initializes the date range and cash. This is a skeleton
### framework you can use for designing an algorithm.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class IndicatorSuiteAlgorithm(QCAlgorithm):
    '''Demonstration algorithm of popular indicators and plotting them.'''

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self._symbol = "SPY"
        self._symbol2 = "GOOG"
        self.custom_symbol = "IBM"
        self.price = None

        self.set_start_date(2013, 1, 1)  #Set Start Date
        self.set_end_date(2014, 12, 31)    #Set End Date
        self.set_cash(25000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration

        self.add_equity(self._symbol, Resolution.DAILY)
        self.add_equity(self._symbol2, Resolution.DAILY)
        self.add_data(CustomData, self.custom_symbol, Resolution.DAILY)

        # Set up default Indicators, these indicators are defined on the Value property of incoming data (except ATR and AROON which use the full TradeBar object)
        self.indicators = {
            'BB' : self.bb(self._symbol, 20, 1, MovingAverageType.SIMPLE, Resolution.DAILY),
            'RSI' : self.rsi(self._symbol, 14, MovingAverageType.SIMPLE, Resolution.DAILY),
            'EMA' : self.ema(self._symbol, 14, Resolution.DAILY),
            'SMA' : self.sma(self._symbol, 14, Resolution.DAILY),
            'MACD' : self.macd(self._symbol, 12, 26, 9, MovingAverageType.SIMPLE, Resolution.DAILY),
            'MOM' : self.mom(self._symbol, 20, Resolution.DAILY),
            'MOMP' : self.momp(self._symbol, 20, Resolution.DAILY),
            'STD' : self.std(self._symbol, 20, Resolution.DAILY),
            # by default if the symbol is a tradebar type then it will be the min of the low property
            'MIN' : self.min(self._symbol, 14, Resolution.DAILY),
            # by default if the symbol is a tradebar type then it will be the max of the high property
            'MAX' : self.max(self._symbol, 14, Resolution.DAILY),
            'ATR' : self.atr(self._symbol, 14, MovingAverageType.SIMPLE, Resolution.DAILY),
            'AROON' : self.aroon(self._symbol, 20, Resolution.DAILY),
            'B' : self.b(self._symbol, self._symbol2, 14)
        }

        #  Here we're going to define indicators using 'selector' functions. These 'selector' functions will define what data gets sent into the indicator
        #  These functions have a signature like the following: decimal Selector(BaseData base_data), and can be defined like: base_data => base_data.value
        #  We'll define these 'selector' functions to select the Low value
        #
        #  For more information on 'anonymous functions' see: http:#en.wikipedia.org/wiki/Anonymous_function
        #                                                     https:#msdn.microsoft.com/en-us/library/bb397687.aspx
        #
        self.selector_indicators = {
            'BB' : self.bb(self._symbol, 20, 1, MovingAverageType.SIMPLE, Resolution.DAILY, Field.low),
            'RSI' :self.rsi(self._symbol, 14, MovingAverageType.SIMPLE, Resolution.DAILY, Field.low),
            'EMA' :self.ema(self._symbol, 14, Resolution.DAILY, Field.low),
            'SMA' :self.sma(self._symbol, 14, Resolution.DAILY, Field.low),
            'MACD' : self.macd(self._symbol, 12, 26, 9, MovingAverageType.SIMPLE, Resolution.DAILY, Field.low),
            'MOM' : self.mom(self._symbol, 20, Resolution.DAILY, Field.low),
            'MOMP' : self.momp(self._symbol, 20, Resolution.DAILY, Field.low),
            'STD' : self.std(self._symbol, 20, Resolution.DAILY, Field.low),
            'MIN' : self.min(self._symbol, 14, Resolution.DAILY, Field.high),
            'MAX' : self.max(self._symbol, 14, Resolution.DAILY, Field.low),
            # ATR and AROON are special in that they accept a TradeBar instance instead of a decimal, we could easily project and/or transform the input TradeBar
            # before it gets sent to the ATR/AROON indicator, here we use a function that will multiply the input trade bar by a factor of two
            'ATR' : self.atr(self._symbol, 14, MovingAverageType.SIMPLE, Resolution.DAILY, Func[IBaseData, IBaseDataBar](self.selector_double__trade_bar)),
            'AROON' : self.aroon(self._symbol, 20, Resolution.DAILY, Func[IBaseData, IBaseDataBar](self.selector_double__trade_bar))
        }

        # Custom Data Indicator:
        self.rsi_custom = self.rsi(self.custom_symbol, 14, MovingAverageType.SIMPLE, Resolution.DAILY)
        self.min_custom = self.min(self.custom_symbol, 14, Resolution.DAILY)
        self.max_custom = self.max(self.custom_symbol, 14, Resolution.DAILY)

        # in addition to defining indicators on a single security, you can all define 'composite' indicators.
        # these are indicators that require multiple inputs. the most common of which is a ratio.
        # suppose we seek the ratio of BTC to SPY, we could write the following:
        spy_close = Identity(self._symbol)
        ibm_close = Identity(self.custom_symbol)

        # this will create a new indicator whose value is IBM/SPY
        self.ratio = IndicatorExtensions.over(ibm_close, spy_close)

        # we can also easily plot our indicators each time they update using th PlotIndicator function
        self.plot_indicator("Ratio", self.ratio)

        # The following methods will add multiple charts to the algorithm output.
        # Those chatrs names will be used later to plot different series in a particular chart.
        # For more information on Lean Charting see: https://www.quantconnect.com/docs#Charting
        Chart('BB')
        Chart('STD')
        Chart('ATR')
        Chart('AROON')
        Chart('MACD')
        Chart('Averages')
        # Here we make use of the Schelude method to update the plots once per day at market close.
        self.schedule.on(self.date_rules.every_day(), self.time_rules.before_market_close(self._symbol), self.update_plots)

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''

        if (#not data.bars.contains_key(self._symbol) or
            not self.indicators['BB'].is_ready or
            not self.indicators['RSI'].is_ready):
            return

        if not data.bars.contains_key(self._symbol):
            return

        self.price = data[self._symbol].close

        if not self.portfolio.hold_stock:
            quantity = int(self.portfolio.cash / self.price)
            self.order(self._symbol, quantity)
            self.debug('Purchased SPY on ' + self.time.strftime('%Y-%m-%d'))

    def update_plots(self):
        if not self.indicators['BB'].is_ready or not self.indicators['STD'].is_ready:
            return

        # Plots can also be created just with this one line command.
        self.plot('RSI', self.indicators['RSI'])
        # Custom data indicator
        self.plot('RSI-FB', self.rsi_custom)

        # Here we make use of the chats decalred in the Initialize method, plotting multiple series
        # in each chart.
        self.plot('STD', 'STD', self.indicators['STD'].current.value)

        self.plot('BB', 'Price', self.price)
        self.plot('BB', 'BollingerUpperBand', self.indicators['BB'].upper_band.current.value)
        self.plot('BB', 'BollingerMiddleBand', self.indicators['BB'].middle_band.current.value)
        self.plot('BB', 'BollingerLowerBand', self.indicators['BB'].lower_band.current.value)


        self.plot('AROON', 'Aroon', self.indicators['AROON'].current.value)
        self.plot('AROON', 'AroonUp', self.indicators['AROON'].aroon_up.current.value)
        self.plot('AROON', 'AroonDown', self.indicators['AROON'].aroon_down.current.value)

        # The following Plot method calls are commented out because of the 10 series limit for backtests
        #self.plot('ATR', 'ATR', self.indicators['ATR'].current.value)
        #self.plot('ATR', 'ATRDoubleBar', self.selector_indicators['ATR'].current.value)
        #self.plot('Averages', 'SMA', self.indicators['SMA'].current.value)
        #self.plot('Averages', 'EMA', self.indicators['EMA'].current.value)
        #self.plot('MOM', self.indicators['MOM'].current.value)
        #self.plot('MOMP', self.indicators['MOMP'].current.value)
        #self.plot('MACD', 'MACD', self.indicators['MACD'].current.value)
        #self.plot('MACD', 'MACDSignal', self.indicators['MACD'].signal.current.value)

    def selector_double__trade_bar(self, bar):
        trade_bar = TradeBar()
        trade_bar.close = 2 * bar.close
        trade_bar.data_type = bar.data_type
        trade_bar.high = 2 * bar.high
        trade_bar.low = 2 * bar.low
        trade_bar.open = 2 * bar.open
        trade_bar.symbol = bar.symbol
        trade_bar.time = bar.time
        trade_bar.value = 2 * bar.value
        trade_bar.period = bar.period
        return trade_bar
