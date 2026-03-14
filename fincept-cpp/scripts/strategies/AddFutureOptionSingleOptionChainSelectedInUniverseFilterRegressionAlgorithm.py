# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-2EF41B6D
# Category: Options
# Description: This regression algorithm tests that we only receive the option chain for a single future contract in the option univ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests that we only receive the option chain for a single future contract
### in the option universe filter.
### </summary>
class AddFutureOptionSingleOptionChainSelectedInUniverseFilterRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.invested = False
        self.on_data_reached = False
        self.option_filter_ran = False
        self.symbols_received = []
        self.expected_symbols_received = []
        self.data_received = {}

        self.set_start_date(2020, 1, 4)
        self.set_end_date(2020, 1, 8)

        self.es = self.add_future(Futures.Indices.SP_500_E_MINI, Resolution.MINUTE, Market.CME)
        self.es.set_filter(lambda future_filter: future_filter.expiration(0, 365).expiration_cycle([3, 6]))

        self.add_future_option(self.es.symbol, self.option_contract_universe_filter_function)

    def option_contract_universe_filter_function(self, option_contracts: OptionFilterUniverse) -> OptionFilterUniverse:
        self.option_filter_ran = True

        expiry = list(set([x.symbol.underlying.id.date for x in option_contracts]))
        expiry = None if not any(expiry) else expiry[0]

        symbol = [x.symbol.underlying for x in option_contracts]
        symbol = None if not any(symbol) else symbol[0]

        if expiry is None or symbol is None:
            raise AssertionError("Expected a single Option contract in the chain, found 0 contracts")

        enumerator = option_contracts.get_enumerator()
        while enumerator.move_next():
            self.expected_symbols_received.append(enumerator.current.symbol)

        return option_contracts

    def on_data(self, data: Slice):
        if not data.has_data:
            return

        self.on_data_reached = True
        has_option_quote_bars = False

        for qb in data.quote_bars.values():
            if qb.symbol.security_type != SecurityType.FUTURE_OPTION:
                continue

            has_option_quote_bars = True

            self.symbols_received.append(qb.symbol)
            if qb.symbol not in self.data_received:
                self.data_received[qb.symbol] = []

            self.data_received[qb.symbol].append(qb)

        if self.invested or not has_option_quote_bars:
            return

        for chain in data.option_chains.values():
            future_invested = False
            option_invested = False

            for option in chain.contracts.keys():
                if future_invested and option_invested:
                    return

                future = option.underlying

                if not option_invested and data.contains_key(option):
                    self.market_order(option, 1)
                    self.invested = True
                    option_invested = True

                if not future_invested and data.contains_key(future):
                    self.market_order(future, 1)
                    self.invested = True
                    future_invested = True

    def on_end_of_algorithm(self):
        super().on_end_of_algorithm()
        self.symbols_received = list(set(self.symbols_received))
        self.expected_symbols_received = list(set(self.expected_symbols_received))

        if not self.option_filter_ran:
            raise AssertionError("Option chain filter was never ran")
        if not self.on_data_reached:
            raise AssertionError("OnData() was never called.")
        if len(self.symbols_received) != len(self.expected_symbols_received):
            raise AssertionError(f"Expected {len(self.expected_symbols_received)} option contracts Symbols, found {len(self.symbols_received)}")

        missing_symbols = [expected_symbol for expected_symbol in self.expected_symbols_received if expected_symbol not in self.symbols_received]
        if any(missing_symbols):
            raise AssertionError(f'Symbols: "{", ".join(missing_symbols)}" were not found in OnData')

        for expected_symbol in self.expected_symbols_received:
            data = self.data_received[expected_symbol]
            for data_point in data:
                data_point.end_time = datetime(1970, 1, 1)

            non_dupe_data_count = len(set(data))
            if non_dupe_data_count < 1000:
                raise AssertionError(f"Received too few data points. Expected >=1000, found {non_dupe_data_count} for {expected_symbol}")
