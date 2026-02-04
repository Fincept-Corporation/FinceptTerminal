# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-DBF9152F
# Category: Forex
# Description: Algorithm demonstrating FOREX asset types and requesting history on them in bulk. As FOREX uses QuoteBars you should ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Algorithm demonstrating FOREX asset types and requesting history on them in bulk. As FOREX uses
### QuoteBars you should request slices
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="history and warm up" />
### <meta name="tag" content="history" />
### <meta name="tag" content="forex" />
class BasicTemplateForexAlgorithm(QCAlgorithm):

    def initialize(self):
        # Set the cash we'd like to use for our backtest
        self.set_cash(100000)

        # Start and end dates for the backtest.
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

        # Add FOREX contract you want to trade
        # find available contracts here https://www.quantconnect.com/data#forex/oanda/cfd
        self.add_forex("EURUSD", Resolution.MINUTE)
        self.add_forex("GBPUSD", Resolution.MINUTE)
        self.add_forex("EURGBP", Resolution.MINUTE)

        self.history(5, Resolution.DAILY)
        self.history(5, Resolution.HOUR)
        self.history(5, Resolution.MINUTE)

        history = self.history(TimeSpan.from_seconds(5), Resolution.SECOND)

        for data in sorted(history, key=lambda x: x.time):
            for key in data.keys():
                self.log(str(key.value) + ": " + str(data.time) + " > " + str(data[key].value))

    def on_data(self, data):
        # Print to console to verify that data is coming in
        for key in data.keys():
            self.log(str(key.value) + ": " + str(data.time) + " > " + str(data[key].value))
