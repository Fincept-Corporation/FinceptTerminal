# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3EE829AC
# Category: Regression Test
# Description: Custom Data Linked Iconic Type Add Data Coarse Selection Regression Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from QuantConnect.Data.Custom.IconicTypes import *

class CustomDataLinkedIconicTypeAddDataCoarseSelectionRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 3, 24)
        self.set_end_date(2014, 4, 7)
        self.set_cash(100000)

        self.universe_settings.resolution = Resolution.DAILY

        self.add_universe_selection(CoarseFundamentalUniverseSelectionModel(self.coarse_selector))

    def coarse_selector(self, coarse):
        symbols = [
            Symbol.create("AAPL", SecurityType.EQUITY, Market.USA),
            Symbol.create("BAC", SecurityType.EQUITY, Market.USA),
            Symbol.create("FB", SecurityType.EQUITY, Market.USA),
            Symbol.create("GOOGL", SecurityType.EQUITY, Market.USA),
            Symbol.create("GOOG", SecurityType.EQUITY, Market.USA),
            Symbol.create("IBM", SecurityType.EQUITY, Market.USA),
        ]

        self.custom_symbols = []

        for symbol in symbols:
            self.custom_symbols.append(self.add_data(LinkedData, symbol, Resolution.DAILY).symbol)

        return symbols

    def on_data(self, data):
        if not self.portfolio.invested and len(self.transactions.get_open_orders()) == 0:
            aapl = Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)
            self.set_holdings(aapl, 0.5)

        for custom_symbol in self.custom_symbols:
            if not self.active_securities.contains_key(custom_symbol.underlying):
                raise Exception(f"Custom data undelrying ({custom_symbol.underlying}) Symbol was not found in active securities")
