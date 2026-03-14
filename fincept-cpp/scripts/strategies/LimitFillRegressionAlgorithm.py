# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C37858CB
# Category: Regression Test
# Description: Basic template algorithm simply initializes the date range and cash
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Basic template algorithm simply initializes the date range and cash
### </summary>
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="limit orders" />
### <meta name="tag" content="placing orders" />
### <meta name="tag" content="updating orders" />
### <meta name="tag" content="regression test" />
class LimitFillRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,7)  #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY", Resolution.SECOND)

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if data.contains_key("SPY"):
            if self.is_round_hour(self.time):
                negative = 1 if self.time < (self.start_date + timedelta(days=2)) else -1
                self.limit_order("SPY", negative*10, data["SPY"].price)

    def is_round_hour(self, date_time):
        '''Verify whether datetime is round hour'''
        return date_time.minute == 0 and date_time.second == 0

    def on_order_event(self, order_event):
        self.debug(str(order_event))
