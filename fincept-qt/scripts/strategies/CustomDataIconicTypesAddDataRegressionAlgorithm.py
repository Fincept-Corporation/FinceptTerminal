# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-52D9893F
# Category: Regression Test
# Description: Regression algorithm checks that adding data via AddData works as expected
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from QuantConnect.Data.Custom.IconicTypes import *

### <summary>
### Regression algorithm checks that adding data via AddData
### works as expected
### </summary>
class CustomDataIconicTypesAddDataRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)

        twx_equity = self.add_equity("TWX", Resolution.DAILY).symbol
        custom_twx_symbol = self.add_data(LinkedData, twx_equity, Resolution.DAILY).symbol

        self.googl_equity = self.add_equity("GOOGL", Resolution.DAILY).symbol
        custom_googl_symbol = self.add_data(LinkedData, "GOOGL", Resolution.DAILY).symbol

        unlinked_data_symbol = self.add_data(UnlinkedData, "GOOGL", Resolution.DAILY).symbol
        unlinked_data_symbol_underlying_equity = Symbol.create("MSFT", SecurityType.EQUITY, Market.USA)
        unlinked_data_symbol_underlying = self.add_data(UnlinkedData, unlinked_data_symbol_underlying_equity, Resolution.DAILY).symbol

        option_symbol = self.add_option("TWX", Resolution.MINUTE).symbol
        custom_option_symbol = self.add_data(LinkedData, option_symbol, Resolution.DAILY).symbol

        if custom_twx_symbol.underlying != twx_equity:
            raise Exception(f"Underlying symbol for {custom_twx_symbol} is not equal to TWX equity. Expected {twx_equity} got {custom_twx_symbol.underlying}")
        if custom_googl_symbol.underlying != self.googl_equity:
            raise Exception(f"Underlying symbol for {custom_googl_symbol} is not equal to GOOGL equity. Expected {self.googl_equity} got {custom_googl_symbol.underlying}")
        if unlinked_data_symbol.has_underlying:
            raise Exception(f"Unlinked data type (no underlying) has underlying when it shouldn't. Found {unlinked_data_symbol.underlying}")
        if not unlinked_data_symbol_underlying.has_underlying:
            raise Exception("Unlinked data type (with underlying) has no underlying Symbol even though we added with Symbol")
        if unlinked_data_symbol_underlying.underlying != unlinked_data_symbol_underlying_equity:
            raise Exception(f"Unlinked data type underlying does not equal equity Symbol added. Expected {unlinked_data_symbol_underlying_equity} got {unlinked_data_symbol_underlying.underlying}")
        if custom_option_symbol.underlying != option_symbol:
            raise Exception(f"Option symbol not equal to custom underlying symbol. Expected {option_symbol} got {custom_option_symbol.underlying}")

        try:
            custom_data_no_cache = self.add_data(LinkedData, "AAPL", Resolution.DAILY)
            raise Exception("AAPL was found in the SymbolCache, though it should be missing")
        except InvalidOperationException as e:
            return

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested and len(self.transactions.get_open_orders()) == 0:
            self.set_holdings(self.googl_equity, 0.5)
