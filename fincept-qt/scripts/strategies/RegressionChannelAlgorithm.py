# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-CB76FD5F
# Category: Regression Test
# Description: Regression Channel algorithm simply initializes the date range and cash
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression Channel algorithm simply initializes the date range and cash
### </summary>
### <meta name="tag" content="indicators" />
### <meta name="tag" content="indicator classes" />
### <meta name="tag" content="placing orders" />
### <meta name="tag" content="plotting indicators" />
class RegressionChannelAlgorithm(QCAlgorithm):

    def initialize(self):

        self.set_cash(100000)
        self.set_start_date(2009,1,1)
        self.set_end_date(2015,1,1)

        equity = self.add_equity("SPY", Resolution.MINUTE)
        self._spy = equity.symbol
        self._holdings = equity.holdings
        self._rc = self.rc(self._spy, 30, 2, Resolution.DAILY)

        stock_plot = Chart("Trade Plot")
        stock_plot.add_series(Series("Buy", SeriesType.SCATTER, 0))
        stock_plot.add_series(Series("Sell", SeriesType.SCATTER, 0))
        stock_plot.add_series(Series("UpperChannel", SeriesType.LINE, 0))
        stock_plot.add_series(Series("LowerChannel", SeriesType.LINE, 0))
        stock_plot.add_series(Series("Regression", SeriesType.LINE, 0))
        self.add_chart(stock_plot)

    def on_data(self, data):
        if (not self._rc.is_ready) or (not data.contains_key(self._spy)): return
        if data[self._spy] is None: return
        value = data[self._spy].value
        if self._holdings.quantity <= 0 and value < self._rc.lower_channel.current.value:
            self.set_holdings(self._spy, 1)
            self.plot("Trade Plot", "Buy", value)
        if self._holdings.quantity >= 0 and value > self._rc.upper_channel.current.value:
            self.set_holdings(self._spy, -1)
            self.plot("Trade Plot", "Sell", value)

    def on_end_of_day(self, symbol):
        self.plot("Trade Plot", "UpperChannel", self._rc.upper_channel.current.value)
        self.plot("Trade Plot", "LowerChannel", self._rc.lower_channel.current.value)
        self.plot("Trade Plot", "Regression", self._rc.linear_regression.current.value)
