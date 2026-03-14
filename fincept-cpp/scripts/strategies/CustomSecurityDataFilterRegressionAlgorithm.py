# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-2F0E8AB5
# Category: Regression Test
# Description: Regression algorithm asserting we can specify a custom security data filter
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting we can specify a custom security data filter
### </summary>
class CustomSecurityDataFilterRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_cash(2500000)
        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,7)

        security = self.add_security(SecurityType.EQUITY, "SPY")
        security.set_data_filter(CustomDataFilter())
        self.data_points = 0

    def on_data(self, data):
        self.data_points += 1
        self.set_holdings("SPY", 0.2)
        if self.data_points > 5:
            raise Exception("There should not be more than 5 data points, but there were " + str(self.data_points))


class CustomDataFilter(SecurityDataFilter):
    def filter(self, vehicle: Security, data: BaseData) -> bool:
        """
        Skip data after 9:35am
        """
        if data.time >= datetime(2013,10,7,9,35,0):
            return False
        else:
            return True
