# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-47C778A4
# Category: Regression Test
# Description: This algorithm demonstrates extended market hours trading
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This algorithm demonstrates extended market hours trading.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="assets" />
### <meta name="tag" content="regression test" />
class ExtendedMarketTradingRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.spy = self.add_equity("SPY", Resolution.MINUTE, Market.USA, True, 1, True)

        self._last_action = None

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if self._last_action is not None and self._last_action.date() == self.time.date():
            return

        spy_bar = data.bars['SPY']

        if not self.in_market_hours():
            self.limit_order("SPY", 10, spy_bar.low)
            self._last_action = self.time

    def on_order_event(self, order_event):
        self.log(str(order_event))
        if self.in_market_hours():
            raise Exception("Order processed during market hours.")

    def in_market_hours(self):
        now = self.time.time()
        open = time(9,30,0)
        close = time(16,0,0)
        return (open < now) and (close > now)

