# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E048C42D
# Category: Warmup
# Description: This algorithm demonstrates using the history provider to retrieve data to warm up indicators before data is received
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This algorithm demonstrates using the history provider to retrieve data
### to warm up indicators before data is received.
### </summary>
### <meta name="tag" content="indicators" />
### <meta name="tag" content="history" />
### <meta name="tag" content="history and warm up" />
### <meta name="tag" content="using data" />
class WarmupHistoryAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2014,5,2)   #Set Start Date
        self.set_end_date(2014,5,2)     #Set End Date
        self.set_cash(100000)          #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        forex = self.add_forex("EURUSD", Resolution.SECOND)
        forex = self.add_forex("NZDUSD", Resolution.SECOND)

        fast_period = 60
        slow_period = 3600
        self.fast = self.ema("EURUSD", fast_period)
        self.slow = self.ema("EURUSD", slow_period)

        # "slow_period + 1" because rolling window waits for one to fall off the back to be considered ready
        # History method returns a dict with a pandas.data_frame
        history = self.history(["EURUSD", "NZDUSD"], slow_period + 1)

        # prints out the tail of the dataframe
        self.log(str(history.loc["EURUSD"].tail()))
        self.log(str(history.loc["NZDUSD"].tail()))

        for index, row in history.loc["EURUSD"].iterrows():
            self.fast.update(index, row["close"])
            self.slow.update(index, row["close"])

        self.log("FAST {0} READY. Samples: {1}".format("IS" if self.fast.is_ready else "IS NOT", self.fast.samples))
        self.log("SLOW {0} READY. Samples: {1}".format("IS" if self.slow.is_ready else "IS NOT", self.slow.samples))


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''

        if self.fast.current.value > self.slow.current.value:
            self.set_holdings("EURUSD", 1)
        else:
            self.set_holdings("EURUSD", -1)
