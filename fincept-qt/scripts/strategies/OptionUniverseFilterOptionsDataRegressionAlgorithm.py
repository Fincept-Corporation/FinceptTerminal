# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-62274EA6
# Category: Options
# Description: Regression algorithm demonstrating the option universe filter feature that allows accessing the option universe data,...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from OptionUniverseFilterGreeksRegressionAlgorithm import OptionUniverseFilterGreeksRegressionAlgorithm

### <summary>
### Regression algorithm demonstrating the option universe filter feature that allows accessing the option universe data,
### including greeks, open interest and implied volatility, and filtering the contracts based on this data.
### </summary>
class OptionUniverseFilterOptionsDataRegressionAlgorithm(OptionUniverseFilterGreeksRegressionAlgorithm):

    def option_filter(self, universe: OptionFilterUniverse) -> OptionFilterUniverse:
        # The filter used for the option security will be equivalent to the following commented one below,
        # but it is more flexible and allows for more complex filtering:

        '''
        return universe \
            .delta(self._min_delta, self._max_delta) \
            .gamma(self._min_gamma, self._max_gamma) \
            .vega(self._min_vega, self._max_vega) \
            .theta(self._min_theta, self._max_theta) \
            .rho(self._min_rho, self._max_rho) \
            .implied_volatility(self._min_iv, self._max_iv) \
            .open_interest(self._min_open_interest, self._max_open_interest)
        '''

        # These contracts list will already be filtered by the strikes and expirations,
        # since those filters where applied before this one.
        return universe \
            .contracts(lambda contracts: [
                contract for contract in contracts
                # Can access the contract data here and do some filtering based on it is needed.
                # More complex math can be done here for filtering, but will be simple here for demonstration sake:
                if (contract.Greeks.Delta > self._min_delta and contract.Greeks.Delta < self._max_delta and
                    contract.Greeks.Gamma > self._min_gamma and contract.Greeks.Gamma < self._max_gamma and
                    contract.Greeks.Vega > self._min_vega and contract.Greeks.Vega < self._max_vega and
                    contract.Greeks.Theta > self._min_theta and contract.Greeks.Theta < self._max_theta and
                    contract.Greeks.Rho > self._min_rho and contract.Greeks.Rho < self._max_rho and
                    contract.ImpliedVolatility > self._min_iv and contract.ImpliedVolatility < self._max_iv and
                    contract.OpenInterest > self._min_open_interest and contract.OpenInterest < self._max_open_interest)
                ])
