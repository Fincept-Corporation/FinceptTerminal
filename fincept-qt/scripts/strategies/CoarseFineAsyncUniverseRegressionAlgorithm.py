# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A0A879A8
# Category: Universe Selection
# Description: Regression algorithm asserting that using separate coarse & fine selection with async universe settings is not allowed
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting that using separate coarse & fine selection with async universe settings is not allowed
### </summary>
class CoarseFineAsyncUniverseRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

        self.universe_settings.asynchronous = True

        threw_exception = False
        try:
            self.add_universe(self.coarse_selection_function, self.fine_selection_function)
        except:
            # expected
            threw_exception = True
            pass

        if not threw_exception:
            raise ValueError("Expected exception to be thrown for AddUniverse")

        self.set_universe_selection(FineFundamentalUniverseSelectionModel(self.coarse_selection_function, self.fine_selection_function))

    def coarse_selection_function(self, coarse):
        return []

    def fine_selection_function(self, fine):
        return []
