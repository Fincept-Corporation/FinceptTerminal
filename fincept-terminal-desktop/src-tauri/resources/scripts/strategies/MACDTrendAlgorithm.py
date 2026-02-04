# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-F328066F
# Category: Momentum
# Description: Simple indicator demonstration algorithm of MACD
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Simple indicator demonstration algorithm of MACD
### </summary>
### <meta name="tag" content="indicators" />
### <meta name="tag" content="indicator classes" />
### <meta name="tag" content="plotting indicators" />
class MACDTrendAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2004, 1, 1)    #Set Start Date
        self.set_end_date(2015, 1, 1)      #Set End Date
        self.set_cash(100000)             #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY", Resolution.DAILY)

        # define our daily macd(12,26) with a 9 day signal
        self.__macd = self.macd("SPY", 12, 26, 9, MovingAverageType.EXPONENTIAL, Resolution.DAILY)
        self.__previous = datetime.min
        self.plot_indicator("MACD", True, self.__macd, self.__macd.signal)
        self.plot_indicator("SPY", self.__macd.fast, self.__macd.slow)


    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        # wait for our macd to fully initialize
        if not self.__macd.is_ready: return

        # only once per day
        if self.__previous.date() == self.time.date(): return

        # define a small tolerance on our checks to avoid bouncing
        tolerance = 0.0025

        holdings = self.portfolio["SPY"].quantity

        signal_delta_percent = (self.__macd.current.value - self.__macd.signal.current.value)/self.__macd.fast.current.value

        # if our macd is greater than our signal, then let's go long
        if holdings <= 0 and signal_delta_percent > tolerance:  # 0.01%
            # longterm says buy as well
            self.set_holdings("SPY", 1.0)

        # of our macd is less than our signal, then let's go short
        elif holdings >= 0 and signal_delta_percent < -tolerance:
            self.liquidate("SPY")


        self.__previous = self.time
