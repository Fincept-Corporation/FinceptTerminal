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

from datetime import datetime
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting that the option chain APIs return consistent values.
### See QCAlgorithm.OptionChain(Symbol) and QCAlgorithm.OptionChainProvider
### </summary>
class OptionChainApisConsistencyRegressionAlgorithm(QCAlgorithm):

    def initialize(self) -> None:
        test_date = self.get_test_date()

        self.set_start_date(test_date)
        self.set_end_date(test_date)

        option = self.get_option()

        option_chain_from_algorithm_api = [x.symbol for x in self.option_chain(option.symbol).contracts.values()]

        exchange_time = Extensions.convert_from_utc(self.utc_time, option.exchange.time_zone)
        option_chain_from_provider_api = list(self.option_chain_provider.get_option_contract_list(option.symbol, exchange_time))

        if len(option_chain_from_algorithm_api) == 0:
            raise AssertionError("No options in chain from algorithm API")

        if len(option_chain_from_provider_api) == 0:
            raise AssertionError("No options in chain from provider API")

        if len(option_chain_from_algorithm_api) != len(option_chain_from_provider_api):
            raise AssertionError(f"Expected {len(option_chain_from_provider_api)} options in chain from provider API, "
                                 f"but got {len(option_chain_from_algorithm_api)}");

        for i in range(len(option_chain_from_algorithm_api)):
            symbol1 = option_chain_from_algorithm_api[i]
            symbol2 = option_chain_from_provider_api[i]

            if symbol1 != symbol2:
                raise AssertionError(f"Expected {symbol2} in chain from provider API, but got {symbol1}")

    def get_test_date(self) -> datetime:
        return datetime(2015, 12, 25)

    def get_option(self) -> Option:
        return self.add_option("GOOG")
