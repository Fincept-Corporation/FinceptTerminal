# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5FF79273
# Category: Regression Test
# Description: Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorith...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

# <summary>
# Regression algorithm to test the behaviour of ARMA versus AR models at the same order of differencing.
# In particular, an ARIMA(1,1,1) and ARIMA(1,1,0) are instantiated while orders are placed if their difference
# is sufficiently large (which would be due to the inclusion of the MA(1) term).
# </summary>
class AutoRegressiveIntegratedMovingAverageRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2013, 1, 7)
        self.set_end_date(2013, 12, 11)
        self.settings.automatic_indicator_warm_up = True
        self.add_equity("SPY", Resolution.DAILY)
        self._arima = self.arima("SPY", 1, 1, 1, 50)
        self._ar = self.arima("SPY", 1, 1, 0, 50)

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if self._arima.is_ready:
            if abs(self._arima.current.value - self._ar.current.value) > 1:
                if self._arima.current.value > self.last:
                    self.market_order("SPY", 1)
                else:
                    self.market_order("SPY", -1)
            self.last = self._arima.current.value
