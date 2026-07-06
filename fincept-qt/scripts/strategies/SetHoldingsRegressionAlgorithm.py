# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-58C330FF
# Category: Regression Test
# Description: Regression algorithm testing the SetHolding trading API precision
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm testing the SetHolding trading API precision
### </summary>
class SetHoldingsRegressionAlgorithm(QCAlgorithm):
    '''Basic template algorithm simply initializes the date range and cash'''

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 8)
        self.add_equity("SPY", Resolution.MINUTE)

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings("SPY", 0.1)
            self.set_holdings("SPY", float(0.20))
            self.set_holdings("SPY", np.float64(0.30))
            self.set_holdings("SPY", 1)
