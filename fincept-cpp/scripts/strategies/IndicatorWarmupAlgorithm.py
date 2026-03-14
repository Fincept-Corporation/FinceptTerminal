# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-935D3B5D
# Category: Indicators
# Description: Regression test for history and warm up using the data available in open source
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression test for history and warm up using the data available in open source.
### </summary>
### <meta name="tag" content="history and warm up" />
### <meta name="tag" content="history" />
### <meta name="tag" content="regression test" />
### <meta name="tag" content="warm up" />
class IndicatorWarmupAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013, 10, 8)   #Set Start Date
        self.set_end_date(2013, 10, 11)    #Set End Date
        self.set_cash(1000000)            #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY")
        self.add_equity("IBM")
        self.add_equity("BAC")
        self.add_equity("GOOG", Resolution.DAILY)
        self.add_equity("GOOGL", Resolution.DAILY)

        self.__sd = { }
        for security in self.securities:
            self.__sd[security.key] = self.SymbolData(security.key, self)

        # we want to warm up our algorithm
        self.set_warmup(self.SymbolData.REQUIRED_BARS_WARMUP)

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        # we are only using warmup for indicator spooling, so wait for us to be warm then continue
        if self.is_warming_up: return

        for sd in self.__sd.values():
            last_price_time = sd.close.current.time
            if self.round_down(last_price_time, sd.security.subscription_data_config.increment):
                sd.update()

    def on_order_event(self, fill):
        sd = self.__sd.get(fill.symbol, None)
        if sd is not None:
            sd.on_order_event(fill)

    def round_down(self, time, increment):
        if increment.days != 0:
            return time.hour == 0 and time.minute == 0 and time.second == 0
        else:
            return time.second == 0

    class SymbolData:
        REQUIRED_BARS_WARMUP = 40
        PERCENT_TOLERANCE = 0.001
        PERCENT_GLOBAL_STOP_LOSS = 0.01
        LOT_SIZE = 10

        def __init__(self, symbol, algorithm):
            self.symbol = symbol
            self.__algorithm = algorithm   # if we're receiving daily

            self.__current_stop_loss = None

            self.security = algorithm.securities[symbol]
            self.close = algorithm.identity(symbol)
            self._adx = algorithm.adx(symbol, 14)
            self._ema = algorithm.ema(symbol, 14)
            self._macd = algorithm.macd(symbol, 12, 26, 9)

            self.is_ready = self.close.is_ready and self._adx.is_ready and self._ema.is_ready and self._macd.is_ready
            self.is_uptrend = False
            self.is_downtrend = False

        def update(self):
            self.is_ready = self.close.is_ready and self._adx.is_ready and self._ema.is_ready and self._macd.is_ready

            tolerance = 1 - self.PERCENT_TOLERANCE
            self.is_uptrend = self._macd.signal.current.value > self._macd.current.value * tolerance and\
                self._ema.current.value > self.close.current.value * tolerance

            self.is_downtrend = self._macd.signal.current.value < self._macd.current.value * tolerance and\
                self._ema.current.value < self.close.current.value * tolerance

            self.try_enter()
            self.try_exit()

        def try_enter(self):
            # can't enter if we're already in
            if self.security.invested: return False

            qty = 0
            limit = 0.0

            if self.is_uptrend:
                # 100 order lots
                qty = self.LOT_SIZE
                limit = self.security.low
            elif self.is_downtrend:
                qty = -self.LOT_SIZE
                limit = self.security.high

            if qty != 0:
                ticket = self.__algorithm.limit_order(self.symbol, qty, limit, "TryEnter at: {0}".format(limit))

        def try_exit(self):
            # can't exit if we haven't entered
            if not self.security.invested: return

            limit = 0
            qty = self.security.holdings.quantity
            exit_tolerance = 1 + 2 * self.PERCENT_TOLERANCE
            if self.security.holdings.is_long and self.close.current.value * exit_tolerance < self._ema.current.value:
                limit = self.security.high
            elif self.security.holdings.is_short and self.close.current.value > self._ema.current.value * exit_tolerance:
                limit = self.security.low

            if limit != 0:
                ticket = self.__algorithm.limit_order(self.symbol, -qty, limit, "TryExit at: {0}".format(limit))

        def on_order_event(self, fill):
            if fill.status != OrderStatus.FILLED: return

            qty = self.security.holdings.quantity

            # if we just finished entering, place a stop loss as well
            if self.security.invested:
                stop = fill.fill_price*(1 - self.PERCENT_GLOBAL_STOP_LOSS) if self.security.holdings.is_long \
                    else fill.fill_price*(1 + self.PERCENT_GLOBAL_STOP_LOSS)

                self.__current_stop_loss = self.__algorithm.stop_market_order(self.symbol, -qty, stop, "StopLoss at: {0}".format(stop))

            # check for an exit, cancel the stop loss
            elif (self.__current_stop_loss is not None and self.__current_stop_loss.status is not OrderStatus.FILLED):
                # cancel our current stop loss
                self.__current_stop_loss.cancel("Exited position")
                self.__current_stop_loss = None
