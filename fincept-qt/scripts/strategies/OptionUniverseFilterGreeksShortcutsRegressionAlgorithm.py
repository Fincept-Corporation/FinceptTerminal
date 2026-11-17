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
