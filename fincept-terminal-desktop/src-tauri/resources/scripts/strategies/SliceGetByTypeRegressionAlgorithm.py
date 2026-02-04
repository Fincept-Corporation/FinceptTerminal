# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-198E83AF
# Category: Regression Test
# Description: Regression algorithm asserting slice.get() works for both the alpha and the algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

trade_flag = False

### <summary>
### Regression algorithm asserting slice.get() works for both the alpha and the algorithm
### </summary>
class SliceGetByTypeRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10,11)

        self.add_equity("SPY", Resolution.MINUTE)
        self.set_alpha(TestAlphaModel())

    def on_data(self, data):
        if "SPY" in data:
            tb = data.get(TradeBar)["SPY"]
            global trade_flag
            if not self.portfolio.invested and trade_flag:
                self.set_holdings("SPY", 1)

class TestAlphaModel(AlphaModel):
    def update(self, algorithm, data):
        insights = []

        if "SPY" in data:
            tb = data.get(TradeBar)["SPY"]
            global trade_flag
            trade_flag = True

        return insights

    def on_securities_changed(self, algorithm, changes):
        return
