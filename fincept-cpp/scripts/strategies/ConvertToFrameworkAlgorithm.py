# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E9C47E95
# Category: General Strategy
# Description: Demonstration algorithm showing how to easily convert an old algorithm into the framework. ###  1. When making orders...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration algorithm showing how to easily convert an old algorithm into the framework.
###
###  1. When making orders, also create insights for the correct direction (up/down/flat), can also set insight prediction period/magnitude/direction
###  2. Emit insights before placing any trades
###  3. Profit :)
###  </summary>
###  <meta name="tag" content="indicators" />
###  <meta name="tag" content="indicator classes" />
###  <meta name="tag" content="plotting indicators" />
class ConvertToFrameworkAlgorithm(QCAlgorithm):
    '''Demonstration algorithm showing how to easily convert an old algorithm into the framework.'''

    fast_ema_period = 12
    slow_ema_period = 26

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2004, 1, 1)
        self.set_end_date(2015, 1, 1)

        self._symbol = self.add_security(SecurityType.EQUITY, 'SPY', Resolution.DAILY).symbol

        # define our daily macd(12,26) with a 9 day signal
        self._macd = self.macd(self._symbol, self.fast_ema_period, self.slow_ema_period, 9, MovingAverageType.EXPONENTIAL, Resolution.DAILY)


    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.
        Args:
            data: Slice object with your stock data'''
        # wait for our indicator to be ready
        if not self._macd.is_ready or not data.contains_key(self._symbol) or data[self._symbol] is None: return

        holding = self.portfolio[self._symbol]

        signal_delta_percent = float(self._macd.current.value - self._macd.signal.current.value) / float(self._macd.fast.current.value)
        tolerance = 0.0025

        # if our macd is greater than our signal, then let's go long
        if holding.quantity <= 0 and signal_delta_percent > tolerance:
            # 1. Call emit_insights with insights created in correct direction, here we're going long
            #    The emit_insights method can accept multiple insights separated by commas
            self.emit_insights(
                # Creates an insight for our symbol, predicting that it will move up within the fast ema period number of days
                Insight.price(self._symbol, timedelta(self.fast_ema_period), InsightDirection.UP)
            )

            # longterm says buy as well
            self.set_holdings(self._symbol, 1)

        # if our macd is less than our signal, then let's go short
        elif holding.quantity >= 0 and signal_delta_percent < -tolerance:
            # 1. Call emit_insights with insights created in correct direction, here we're going short
            #    The emit_insights method can accept multiple insights separated by commas
            self.emit_insights(
                # Creates an insight for our symbol, predicting that it will move down within the fast ema period number of days
                Insight.price(self._symbol, timedelta(self.fast_ema_period), InsightDirection.DOWN)
            )

            self.set_holdings(self._symbol, -1)

        # if we wanted to liquidate our positions
        ## 1. Call emit_insights with insights create in the correct direction -- Flat

        #self.emit_insights(
            # Creates an insight for our symbol, predicting that it will move down or up within the fast ema period number of days, depending on our current position
            # Insight.price(self._symbol, timedelta(self.fast_ema_period), InsightDirection.FLAT)
        #)

        # self.liquidate()

        # plot both lines
        self.plot("MACD", self._macd, self._macd.signal)
        self.plot(self._symbol.value, self._macd.fast, self._macd.slow)
        self.plot(self._symbol.value, "Open", data[self._symbol].open)
