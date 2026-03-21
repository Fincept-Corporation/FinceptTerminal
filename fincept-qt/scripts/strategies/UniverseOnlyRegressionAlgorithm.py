# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-953A6967
# Category: Universe Selection
# Description: Asserts that algorithms can be universe-only, that is, universe selection is performed even if the ETF security is no...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Asserts that algorithms can be universe-only, that is, universe selection is performed even if the ETF security is not explicitly added.
### Reproduces https://github.com/QuantConnect/Lean/issues/7473
### </summary>
class UniverseOnlyRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 12, 1)
        self.set_end_date(2020, 12, 12)
        self.set_cash(100000)

        self.universe_settings.resolution = Resolution.DAILY

        # Add universe without a security added
        self.add_universe(self.universe.etf("GDVD", self.universe_settings, self.filter_universe))

        self.selection_done = False

    def filter_universe(self, constituents: List[ETFConstituentData]) -> List[Symbol]:
        self.selection_done = True
        return [x.symbol for x in constituents]

    def on_end_of_algorithm(self):
        if not self.selection_done:
            raise Exception("Universe selection was not performed")
