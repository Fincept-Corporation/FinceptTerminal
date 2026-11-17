# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

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
