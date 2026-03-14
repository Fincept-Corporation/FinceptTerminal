# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-DAAB71C9
# Category: General Strategy
# Description: Algorithm demonstrating custom charting support in QuantConnect. The entire charting system of quantconnect is adapta...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Algorithm demonstrating custom charting support in QuantConnect.
### The entire charting system of quantconnect is adaptable. You can adjust it to draw whatever you'd like.
### Charts can be stacked, or overlayed on each other. Series can be candles, lines or scatter plots.
### Even the default behaviours of QuantConnect can be overridden.
### </summary>
### <meta name="tag" content="charting" />
### <meta name="tag" content="adding charts" />
### <meta name="tag" content="series types" />
### <meta name="tag" content="plotting indicators" />
class CustomChartingAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2016,1,1)
        self.set_end_date(2017,1,1)
        self.set_cash(100000)

        spy = self.add_equity("SPY", Resolution.DAILY).symbol

        # In your initialize method:
        # Chart - Master Container for the Chart:
        stock_plot = Chart("Trade Plot")
        # On the Trade Plotter Chart we want 3 series: trades and price:
        stock_plot.add_series(Series("Buy", SeriesType.SCATTER, 0))
        stock_plot.add_series(Series("Sell", SeriesType.SCATTER, 0))
        stock_plot.add_series(Series("Price", SeriesType.LINE, 0))
        self.add_chart(stock_plot)

        # On the Average Cross Chart we want 2 series, slow MA and fast MA
        avg_cross = Chart("Average Cross")
        avg_cross.add_series(Series("FastMA", SeriesType.LINE, 0))
        avg_cross.add_series(Series("SlowMA", SeriesType.LINE, 0))
        self.add_chart(avg_cross)

        # There's support for candlestick charts built-in:
        weekly_spy_plot = Chart("Weekly SPY")
        spy_candlesticks = CandlestickSeries("SPY")
        weekly_spy_plot.add_series(spy_candlesticks)
        self.add_chart(weekly_spy_plot)

        self.consolidate(spy, Calendar.WEEKLY, lambda bar: self.plot("Weekly SPY", "SPY", bar))

        self.fast_ma = 0
        self.slow_ma = 0
        self.last_price = 0
        self.resample = datetime.min
        self.resample_period = (self.end_date - self.start_date) / 2000

    def on_data(self, slice):
        if slice["SPY"] is None: return

        self.last_price = slice["SPY"].close
        if self.fast_ma == 0: self.fast_ma = self.last_price
        if self.slow_ma == 0: self.slow_ma = self.last_price
        self.fast_ma = (0.01 * self.last_price) + (0.99 * self.fast_ma)
        self.slow_ma = (0.001 * self.last_price) + (0.999 * self.slow_ma)


        if self.time > self.resample:
            self.resample = self.time  + self.resample_period
            self.plot("Average Cross", "FastMA", self.fast_ma)
            self.plot("Average Cross", "SlowMA", self.slow_ma)

        # On the 5th days when not invested buy:
        if not self.portfolio.invested and self.time.day % 13 == 0:
        	self.order("SPY", (int)(self.portfolio.margin_remaining / self.last_price))
        	self.plot("Trade Plot", "Buy", self.last_price)
        elif self.time.day % 21 == 0 and self.portfolio.invested:
            self.plot("Trade Plot", "Sell", self.last_price)
            self.liquidate()

    def on_end_of_day(self, symbol):
       #Log the end of day prices:
       self.plot("Trade Plot", "Price", self.last_price)
