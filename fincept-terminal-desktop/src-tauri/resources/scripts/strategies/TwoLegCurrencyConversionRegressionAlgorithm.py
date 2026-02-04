# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-FED4E182
# Category: Regression Test
# Description: Regression algorithm which tests that a two leg currency conversion happens correctly
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm which tests that a two leg currency conversion happens correctly
### </summary>
class TwoLegCurrencyConversionRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2018, 4, 4)
        self.set_end_date(2018, 4, 4)
        self.set_brokerage_model(BrokerageName.GDAX, AccountType.CASH)
        # GDAX doesn't have LTCETH or ETHLTC, but they do have ETHUSD and LTCUSD to form a path between ETH and LTC
        self.set_account_currency("ETH")
        self.set_cash("ETH", 100000)
        self.set_cash("LTC", 100000)
        self.set_cash("USD", 100000)

        self._eth_usd_symbol = self.add_crypto("ETHUSD", Resolution.MINUTE).symbol
        self._ltc_usd_symbol = self.add_crypto("LTCUSD", Resolution.MINUTE).symbol

    def on_data(self, data):
        if not self.portfolio.invested:
            self.market_order(self._ltc_usd_symbol, 1)

    def on_end_of_algorithm(self):
        ltc_cash = self.portfolio.cash_book["LTC"]

        conversion_symbols = [x.symbol for x in ltc_cash.currency_conversion.conversion_rate_securities]

        if len(conversion_symbols) != 2:
            raise ValueError(
                f"Expected two conversion rate securities for LTC to ETH, is {len(conversion_symbols)}")

        if conversion_symbols[0] != self._ltc_usd_symbol:
            raise ValueError(
                f"Expected first conversion rate security from LTC to ETH to be {self._ltc_usd_symbol}, is {conversion_symbols[0]}")

        if conversion_symbols[1] != self._eth_usd_symbol:
            raise ValueError(
                f"Expected second conversion rate security from LTC to ETH to be {self._eth_usd_symbol}, is {conversion_symbols[1]}")

        ltc_usd_value = self.securities[self._ltc_usd_symbol].get_last_data().value
        eth_usd_value = self.securities[self._eth_usd_symbol].get_last_data().value

        expected_conversion_rate = ltc_usd_value / eth_usd_value
        actual_conversion_rate = ltc_cash.conversion_rate

        if actual_conversion_rate != expected_conversion_rate:
            raise ValueError(
                f"Expected conversion rate from LTC to ETH to be {expected_conversion_rate}, is {actual_conversion_rate}")
