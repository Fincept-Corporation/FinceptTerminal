# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3D4D4FC7
# Category: General Strategy
# Description: Demonstration of the parameter system of QuantConnect. Using parameters you can pass the values required into C# algo...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of the parameter system of QuantConnect. Using parameters you can pass the values required into C# algorithms for optimization.
### </summary>
### <meta name="tag" content="optimization" />
### <meta name="tag" content="using quantconnect" />
class ParameterizedAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013, 10, 7)   #Set Start Date
        self.set_end_date(2013, 10, 11)    #Set End Date
        self.set_cash(100000)             #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY")

        # Receive parameters from the Job
        fast_period = self.get_parameter("ema-fast", 100)
        slow_period = self.get_parameter("ema-slow", 200)

        self.fast = self.ema("SPY", fast_period)
        self.slow = self.ema("SPY", slow_period)


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''

        # wait for our indicators to ready
        if not self.fast.is_ready or not self.slow.is_ready:
            return

        fast = self.fast.current.value
        slow = self.slow.current.value

        if fast > slow * 1.001:
            self.set_holdings("SPY", 1)
        elif fast < slow * 0.999:
            self.liquidate("SPY")
