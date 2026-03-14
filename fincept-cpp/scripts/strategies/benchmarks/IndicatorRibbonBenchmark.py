# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D5F0AB7B
# Category: Benchmark
# Description: Indicator Ribbon Benchmark
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class IndicatorRibbonBenchmark(QCAlgorithm):

    # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    def initialize(self):
        self.set_start_date(2010, 1, 1)  #Set Start Date
        self.set_end_date(2018, 1, 1)    #Set End Date
        self.spy = self.add_equity("SPY", Resolution.MINUTE).symbol
        count = 50
        offset = 5
        period = 15
        self.ribbon = []
        # define our sma as the base of the ribbon
        self.sma = SimpleMovingAverage(period)
        
        for x in range(count):
            # define our offset to the zero sma, these various offsets will create our 'displaced' ribbon
            delay = Delay(offset*(x+1))
            # define an indicator that takes the output of the sma and pipes it into our delay indicator
            delayed_sma = IndicatorExtensions.of(delay, self.sma)
            # register our new 'delayed_sma' for automatic updates on a daily resolution
            self.register_indicator(self.spy, delayed_sma, Resolution.DAILY)
            self.ribbon.append(delayed_sma)

    def on_data(self, data):
        # wait for our entire ribbon to be ready
        if not all(x.is_ready for x in self.ribbon): return
        for x in self.ribbon:
            value = x.current.value
