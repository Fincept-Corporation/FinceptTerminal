# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5C2FCFA1
# Category: Regression Test
# Description: A demonstration algorithm to check there can be placed an order of a pair not present in the brokerage using the conv...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### A demonstration algorithm to check there can be placed an order of a pair not present
### in the brokerage using the conversion between stablecoins
### </summary>
class StableCoinsRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2018, 5, 1)
        self.set_end_date(2018, 5, 2)
        self.set_cash("USDT", 200000000)
        self.set_brokerage_model(BrokerageName.BINANCE, AccountType.CASH)
        self.add_crypto("BTCUSDT", Resolution.HOUR, Market.BINANCE)

    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings("BTCUSDT", 1)
