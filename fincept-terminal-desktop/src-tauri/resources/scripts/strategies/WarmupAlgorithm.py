# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-DB342081
# Category: Warmup
# Description: Demonstration algorithm for the Warm Up feature with basic indicators
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration algorithm for the Warm Up feature with basic indicators.
### </summary>
### <meta name="tag" content="indicators" />
### <meta name="tag" content="warm up" />
### <meta name="tag" content="history and warm up" />
### <meta name="tag" content="using data" />
class WarmupAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,8)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY", Resolution.SECOND)

        fast_period = 60
        slow_period = 3600

        self.fast = self.EMA("SPY", fast_period)
        self.slow = self.EMA("SPY", slow_period)

        self.set_warmup(slow_period)
        self.first = True


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if self.first and not self.is_warming_up:
            self.first = False
            self.log("Fast: {0}".format(self.fast.samples))
            self.log("Slow: {0}".format(self.slow.samples))

        if self.fast.current.value > self.slow.current.value:
            self.set_holdings("SPY", 1)
        else:
            self.set_holdings("SPY", -1)
