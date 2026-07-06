# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-B0BD73CA
# Category: General Strategy
# Description: This example algorithm shows how to import and use Tiingo daily prices data
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from QuantConnect.Data.Custom.Tiingo import *

### <summary>
### This example algorithm shows how to import and use Tiingo daily prices data.
### </summary>
### <meta name="tag" content="strategy example" />
### <meta name="tag" content="using data" />
### <meta name="tag" content="custom data" />
### <meta name="tag" content="tiingo" />
class TiingoPriceAlgorithm(QCAlgorithm):

    def initialize(self):
        # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
        self.set_start_date(2017, 1, 1)
        self.set_end_date(2017, 12, 31)
        self.set_cash(100000)

        # Set your Tiingo API Token here
        Tiingo.set_auth_code("my-tiingo-api-token")

        self.ticker = "AAPL"
        self.equity = self.add_equity(self.ticker).symbol
        self.aapl = self.add_data(TiingoPrice, self.ticker, Resolution.DAILY).symbol

        self.ema_fast = self.ema(self.equity, 5)
        self.ema_slow = self.ema(self.equity, 10)


    def on_data(self, slice):
        # OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        if not slice.contains_key(self.ticker): return

        # Extract Tiingo data from the slice
        row = slice[self.ticker]

        if row is not None:
            if self.ema_fast.is_ready and self.ema_slow.is_ready:
                self.log(f"{self.time} - {row.symbol.value} - {row.close} {row.value} {row.price} - EmaFast:{self.ema_fast} - EmaSlow:{self.ema_slow}")

            # Simple EMA cross
            if not self.portfolio.invested and self.ema_fast > self.ema_slow:
                self.set_holdings(self.equity, 1)

            elif self.portfolio.invested and self.ema_fast < self.ema_slow:
                self.liquidate(self.equity)
