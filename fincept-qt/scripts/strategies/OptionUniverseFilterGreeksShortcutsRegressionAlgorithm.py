# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-AED01B14
# Category: Options
# Description: Regression algorithm demonstrating the option universe filter by greeks and other options data feature
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from OptionUniverseFilterGreeksRegressionAlgorithm import OptionUniverseFilterGreeksRegressionAlgorithm

### <summary>
### Regression algorithm demonstrating the option universe filter by greeks and other options data feature
### </summary>
class OptionUniverseFilterGreeksShortcutsRegressionAlgorithm(OptionUniverseFilterGreeksRegressionAlgorithm):

    def option_filter(self, universe: OptionFilterUniverse) -> OptionFilterUniverse:
        # Contracts can be filtered by greeks, implied volatility, open interest:
        return universe \
            .d(self._min_delta, self._max_delta) \
            .g(self._min_gamma, self._max_gamma) \
            .v(self._min_vega, self._max_vega) \
            .t(self._min_theta, self._max_theta) \
            .r(self._min_rho, self._max_rho) \
            .iv(self._min_iv, self._max_iv) \
            .oi(self._min_open_interest, self._max_open_interest)
