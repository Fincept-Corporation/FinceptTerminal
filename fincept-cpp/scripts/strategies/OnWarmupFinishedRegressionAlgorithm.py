# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-BA4498F3
# Category: Regression Test
# Description: Regression algorithm asserting 'OnWarmupFinished' is being called
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting 'OnWarmupFinished' is being called
### </summary>
class OnWarmupFinishedRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10, 8)  #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self.add_equity("SPY", Resolution.MINUTE)
        self.set_warmup(timedelta(days = 1))
        self._on_warmup_finished = 0
    
    def on_warmup_finished(self):
        self._on_warmup_finished += 1
    
    def on_end_of_algorithm(self):
        if self._on_warmup_finished != 1:
            raise Exception(f"Unexpected OnWarmupFinished call count {self._on_warmup_finished}")
