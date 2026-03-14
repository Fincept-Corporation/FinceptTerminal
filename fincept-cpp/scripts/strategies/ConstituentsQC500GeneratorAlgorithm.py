# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-EFBD246D
# Category: General Strategy
# Description: Demonstration of how to estimate constituents of QC500 index based on the company fundamentals The algorithm creates ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of how to estimate constituents of QC500 index based on the company fundamentals
### The algorithm creates a default tradable and liquid universe containing 500 US equities
### which are chosen at the first trading day of each month.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
### <meta name="tag" content="fine universes" />
class ConstituentsQC500GeneratorAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.universe_settings.resolution = Resolution.DAILY

        self.set_start_date(2018, 1, 1)   # Set Start Date
        self.set_end_date(2019, 1, 1)     # Set End Date
        self.set_cash(100000)            # Set Strategy Cash

        # Add QC500 Universe
        self.add_universe(self.universe.qc_500)
