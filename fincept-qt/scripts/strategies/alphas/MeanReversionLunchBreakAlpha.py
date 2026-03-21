# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-EAEEE69C
# Category: Alpha Model
# Description: Uses the price return between the close of previous day to 12:00 the day after to
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

#
# Academic research suggests that stock market participants generally place their orders at the market open and close.
# Intraday trading volume is J-Shaped, where the minimum trading volume of the day is during lunch-break. Stocks become
# more volatile as order flow is reduced and tend to mean-revert during lunch-break.
#
# This alpha aims to capture the mean-reversion effect of ETFs during lunch-break by ranking 20 ETFs
# on their return between the close of the previous day to 12:00 the day after and predicting mean-reversion
# in price during lunch-break.
#
# Source:  Lunina, V. (June 2011). The Intraday Dynamics of Stock Returns and Trading Activity: Evidence from OMXS 30 (Master's Essay, Lund University).
# Retrieved from http://lup.lub.lu.se/luur/download?func=downloadFile&recordOId=1973850&fileOId=1973852
#
# This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open sourced so the community and client funds can see an example of an alpha.
#

class MeanReversionLunchBreakAlpha(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2018, 1, 1)

        self.set_cash(100000)

        # Set zero transaction fees
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))

        # Use Hourly Data For Simplicity
        self.universe_settings.resolution = Resolution.HOUR
        self.set_universe_selection(CoarseFundamentalUniverseSelectionModel(self.coarse_selection_function))

        # Use MeanReversionLunchBreakAlphaModel to establish insights
        self.set_alpha(MeanReversionLunchBreakAlphaModel())

        # Equally weigh securities in portfolio, based on insights
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        # Set Immediate Execution Model
        self.set_execution(ImmediateExecutionModel())

        # Set Null Risk Management Model
        self.set_risk_management(NullRiskManagementModel())

    # Sort the data by daily dollar volume and take the top '20' ETFs
    def coarse_selection_function(self, coarse):
        sorted_by_dollar_volume = sorted(coarse, key=lambda x: x.dollar_volume, reverse=True)
        filtered = [ x.symbol for x in sorted_by_dollar_volume if not x.has_fundamental_data ]
        return filtered[:20]


class MeanReversionLunchBreakAlphaModel(AlphaModel):
    '''Uses the price return between the close of previous day to 12:00 the day after to
    predict mean-reversion of stock price during lunch break and creates direction prediction
    for insights accordingly.'''

    def __init__(self, *args, **kwargs):
        lookback = kwargs['lookback'] if 'lookback' in kwargs else 1
        self.resolution = Resolution.HOUR
        self.prediction_interval = Time.multiply(Extensions.to_time_span(self.resolution), lookback)
        self._symbol_data_by_symbol = dict()

    def update(self, algorithm, data):

        for symbol, symbol_data in self._symbol_data_by_symbol.items():
            if data.bars.contains_key(symbol):
                bar = data.bars.get_value(symbol)
                symbol_data.update(bar.end_time, bar.close)

        return [] if algorithm.time.hour != 12 else \
               [x.insight for x in self._symbol_data_by_symbol.values()]

    def on_securities_changed(self, algorithm, changes):
        for security in changes.removed_securities:
            self._symbol_data_by_symbol.pop(security.symbol, None)

        # Retrieve price history for all securities in the security universe
        # and update the indicators in the SymbolData object
        symbols = [x.symbol for x in changes.added_securities]
        history = algorithm.history(symbols, 1, self.resolution)
        if history.empty:
            algorithm.debug(f"No data on {algorithm.time}")
            return
        history = history.close.unstack(level = 0)

        for ticker, values in history.items():
            symbol = next((x for x in symbols if str(x) == ticker ), None)
            if symbol in self._symbol_data_by_symbol or symbol is None: continue
            self._symbol_data_by_symbol[symbol] = self.SymbolData(symbol, self.prediction_interval)
            self._symbol_data_by_symbol[symbol].update(values.index[0], values[0])


    class SymbolData:
        def __init__(self, symbol, period):
            self._symbol = symbol
            self.period = period
            # Mean value of returns for magnitude prediction
            self.mean_of_price_change = IndicatorExtensions.sma(RateOfChangePercent(1),3)
            # Price change from close price the previous day
            self.price_change = RateOfChangePercent(3)

        def update(self, time, value):
            return self.mean_of_price_change.update(time, value) and \
                   self.price_change.update(time, value)

        @property
        def insight(self):
            direction = InsightDirection.DOWN if self.price_change.current.value > 0 else InsightDirection.UP
            margnitude = abs(self.mean_of_price_change.current.value)
            return Insight.price(self._symbol, self.period, direction, margnitude, None)
