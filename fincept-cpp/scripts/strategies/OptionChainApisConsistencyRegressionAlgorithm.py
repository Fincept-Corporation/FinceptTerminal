# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A5BE1849
# Category: Options
# Description: Regression algorithm asserting that the option chain APIs return consistent values. See QCAlgorithm.OptionChain(Symbo...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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
