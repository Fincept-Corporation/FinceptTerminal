# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3CCF5CE2
# Category: Options
# Description: This regression algorithm tests that we receive the expected data when we add future option contracts individually us...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests that we receive the expected data when
### we add future option contracts individually using <see cref="AddFutureOptionContract"/>
### </summary>
class AddFutureOptionContractDataStreamingRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.on_data_reached = False
        self.invested = False
        self.symbols_received = []
        self.expected_symbols_received = []
        self.data_received = {}

        self.set_start_date(2020, 1, 4)
        self.set_end_date(2020, 1, 8)

        self.es20h20 = self.add_future_contract(
            Symbol.create_future(Futures.Indices.SP_500_E_MINI, Market.CME, datetime(2020, 3, 20)),
            Resolution.MINUTE).symbol

        self.es19m20 = self.add_future_contract(
            Symbol.create_future(Futures.Indices.SP_500_E_MINI, Market.CME, datetime(2020, 6, 19)),
            Resolution.MINUTE).symbol

        # Get option contract lists for 2020/01/05 (timedelta(days=1)) because Lean has local data for that date
        option_chains = self.option_chain_provider.get_option_contract_list(self.es20h20, self.time + timedelta(days=1))
        option_chains += self.option_chain_provider.get_option_contract_list(self.es19m20, self.time + timedelta(days=1))

        for option_contract in option_chains:
            self.expected_symbols_received.append(self.add_future_option_contract(option_contract, Resolution.MINUTE).symbol)

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

        if data.contains_key(self.es20h20) and data.contains_key(self.es19m20):
            self.set_holdings(self.es20h20, 0.2)
            self.set_holdings(self.es19m20, 0.2)

            self.invested = True

    def on_end_of_algorithm(self):
        self.symbols_received = list(set(self.symbols_received))
        self.expected_symbols_received = list(set(self.expected_symbols_received))

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
