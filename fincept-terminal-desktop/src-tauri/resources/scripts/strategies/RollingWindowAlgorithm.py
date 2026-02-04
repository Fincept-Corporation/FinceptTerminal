# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5A71E57F
# Category: General Strategy
# Description: Using rolling windows for efficient storage of historical data; which automatically clears after a period of time
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Using rolling windows for efficient storage of historical data; which automatically clears after a period of time.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="history and warm up" />
### <meta name="tag" content="history" />
### <meta name="tag" content="warm up" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="rolling windows" />
class RollingWindowAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,1)  #Set Start Date
        self.set_end_date(2013,11,1)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY", Resolution.DAILY)

        # Creates a Rolling Window indicator to keep the 2 TradeBar
        self.window = RollingWindow[TradeBar](2)    # For other security types, use QuoteBar

        # Creates an indicator and adds to a rolling window when it is updated
        self.sma = self.SMA("SPY", 5)
        self.sma.updated += self.sma_updated
        self.sma_win = RollingWindow[IndicatorDataPoint](5)


    def sma_updated(self, sender, updated):
        '''Adds updated values to rolling window'''
        self.sma_win.add(updated)


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''

        # Add SPY TradeBar in rollling window
        self.window.add(data["SPY"])

        # Wait for windows to be ready.
        if not (self.window.is_ready and self.sma_win.is_ready): return

        curr_bar = self.window[0]                     # Current bar had index zero.
        past_bar = self.window[1]                     # Past bar has index one.
        self.log("Price: {0} -> {1} ... {2} -> {3}".format(past_bar.time, past_bar.close, curr_bar.time, curr_bar.close))

        curr_sma = self.sma_win[0]                     # Current SMA had index zero.
        past_sma = self.sma_win[self.sma_win.count-1]   # Oldest SMA has index of window count minus 1.
        self.log("SMA:   {0} -> {1} ... {2} -> {3}".format(past_sma.time, past_sma.value, curr_sma.time, curr_sma.value))

        if not self.portfolio.invested and curr_sma.value > past_sma.value:
            self.set_holdings("SPY", 1)
