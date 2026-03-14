# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-4E7CF48A
# Category: Options
# Description: This algorithm is a regression test for issue #2018 and PR #2038
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This algorithm is a regression test for issue #2018 and PR #2038.
### </summary>
class OptionDataNullReferenceRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2016, 12, 1)
        self.set_end_date(2017, 1, 1)
        self.set_cash(500000)

        self.add_equity("DUST")

        option = self.add_option("DUST")

        option.set_filter(self.universe_func)

    def universe_func(self, universe):
        return universe.include_weeklys().strikes(-1, +1).expiration(timedelta(25), timedelta(100))
