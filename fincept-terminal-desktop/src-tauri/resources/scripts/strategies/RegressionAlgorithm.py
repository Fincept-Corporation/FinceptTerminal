# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D350E387
# Category: Regression Test
# Description: Algorithm used for regression tests purposes
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Algorithm used for regression tests purposes
### </summary>
### <meta name="tag" content="regression test" />
class RegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(10000000)         #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY", Resolution.TICK)
        self.add_equity("BAC", Resolution.MINUTE)
        self.add_equity("AIG", Resolution.HOUR)
        self.add_equity("IBM", Resolution.DAILY)

        self.__last_trade_ticks = self.start_date
        self.__last_trade_trade_bars = self.__last_trade_ticks
        self.__trade_every = timedelta(minutes=1)


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if self.time - self.__last_trade_trade_bars < self.__trade_every:
            return
        self.__last_trade_trade_bars = self.time

        for kvp in data.bars:
            bar = kvp.Value
            if bar.is_fill_forward:
                continue

            symbol = kvp.key
            holdings = self.portfolio[symbol]

            if not holdings.invested:
                self.market_order(symbol, 10)
            else:
                self.market_order(symbol, -holdings.quantity)
