# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-824A47D1
# Category: Regression Test
# Description: Regression algorithm asserting that historical data can be requested with tick resolution without requiring a tick re...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from datetime import timedelta
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting that historical data can be requested with tick resolution without requiring
### a tick resolution subscription
### </summary>
class TickHistoryRequestWithoutTickSubscriptionRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 8)
        self.set_end_date(2013, 10, 8)

        # Subscribing SPY and IBM with daily and hour resolution instead of tick resolution
        spy = self.add_equity("SPY", Resolution.DAILY).symbol
        ibm = self.add_equity("IBM", Resolution.HOUR).symbol

        # Requesting history for SPY and IBM (separately) with tick resolution
        spy_history = self.history[Tick](spy, timedelta(days=1), Resolution.TICK)
        if len(list(spy_history)) == 0:
            raise Exception("SPY tick history is empty")

        ibm_history = self.history[Tick](ibm, timedelta(days=1), Resolution.TICK)
        if len(list(ibm_history)) == 0:
            raise Exception("IBM tick history is empty")

        # Requesting history for SPY and IBM (together) with tick resolution
        spy_ibm_history = self.history[Tick]([spy, ibm], timedelta(days=1), Resolution.TICK)
        if len(list(spy_ibm_history)) == 0:
            raise Exception("Compound SPY and IBM tick history is empty")

        self.quit()
