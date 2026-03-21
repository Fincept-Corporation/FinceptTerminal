# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-61D659F8
# Category: Options
# Description: Regression algorithm testing history requests for <see cref="OptionUniverse"/> type work as expected and return the s...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm testing history requests for <see cref="OptionUniverse"/> type work as expected
### and return the same data as the option chain provider.
### </summary>
class OptionUniverseHistoryRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 12, 25)
        self.set_end_date(2015, 12, 25)

        option = self.add_option("GOOG").symbol

        historical_options_data_df = self.history(option, 3, flatten=True)

        # Level 0 of the multi-index is the date, we expect 3 dates, 3 option chains
        if historical_options_data_df.index.levshape[0] != 3:
            raise RegressionTestException(f"Expected 3 option chains from history request, but got {historical_options_data_df.index.levshape[1]}")

        for date in historical_options_data_df.index.levels[0]:
            expected_chain = list(self.option_chain_provider.get_option_contract_list(option, date))
            expected_chain_count = len(expected_chain)

            actual_chain = historical_options_data_df.loc[date]
            actual_chain_count = len(actual_chain)

            if expected_chain_count != actual_chain_count:
                raise RegressionTestException(f"Expected {expected_chain_count} options in chain on {date}, but got {actual_chain_count}")

            for i, symbol in enumerate(actual_chain.index):
                expected_symbol = expected_chain[i]
                if symbol != expected_symbol:
                    raise RegressionTestException(f"Expected symbol {expected_symbol} at index {i} on {date}, but got {symbol}")
