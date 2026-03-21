# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-876722D3
# Category: Options
# Description: Regression algorithm demonstrating the option universe filter by greeks and other options data feature
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm demonstrating the option universe filter by greeks and other options data feature
### </summary>
class OptionUniverseFilterGreeksRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(100000)

        underlying_ticker = "GOOG"
        self.add_equity(underlying_ticker)
        option = self.add_option(underlying_ticker)
        self.option_symbol = option.symbol

        self._min_delta = 0.5
        self._max_delta = 1.5
        self._min_gamma = 0.0001
        self._max_gamma = 0.0006
        self._min_vega = 0.01
        self._max_vega = 1.5
        self._min_theta = -2.0
        self._max_theta = -0.5
        self._min_rho = 0.5
        self._max_rho = 3.0
        self._min_iv = 1.0
        self._max_iv = 3.0
        self._min_open_interest = 100
        self._max_open_interest = 500

        option.set_filter(self.main_filter)
        self.option_chain_received = False

    def main_filter(self, universe: OptionFilterUniverse) -> OptionFilterUniverse:
        total_contracts = len(list(universe))

        filtered_universe = self.option_filter(universe)
        filtered_contracts = len(list(filtered_universe))

        if filtered_contracts == total_contracts:
            raise RegressionTestException(f"Expected filtered universe to have less contracts than original universe. "
                                          f"Filtered contracts count ({filtered_contracts}) is equal to total contracts count ({total_contracts})")

        return filtered_universe

    def option_filter(self, universe: OptionFilterUniverse) -> OptionFilterUniverse:
        # Contracts can be filtered by greeks, implied volatility, open interest:
        return universe \
            .delta(self._min_delta, self._max_delta) \
            .gamma(self._min_gamma, self._max_gamma) \
            .vega(self._min_vega, self._max_vega) \
            .theta(self._min_theta, self._max_theta) \
            .rho(self._min_rho, self._max_rho) \
            .implied_volatility(self._min_iv, self._max_iv) \
            .open_interest(self._min_open_interest, self._max_open_interest)

        # Note: there are also shortcuts for these filter methods:
        '''
        return universe \
            .d(self._min_delta, self._max_delta) \
            .g(self._min_gamma, self._max_gamma) \
            .v(self._min_vega, self._max_vega) \
            .t(self._min_theta, self._max_theta) \
            .r(self._min_rho, self._max_rho) \
            .iv(self._min_iv, self._max_iv) \
            .oi(self._min_open_interest, self._max_open_interest)
        '''

    def on_data(self, slice: Slice) -> None:
        chain = slice.option_chains.get(self.option_symbol)
        if chain and len(chain.contracts) > 0:
            self.option_chain_received = True

    def on_end_of_algorithm(self) -> None:
        if not self.option_chain_received:
            raise RegressionTestException("Option chain was not received.")
