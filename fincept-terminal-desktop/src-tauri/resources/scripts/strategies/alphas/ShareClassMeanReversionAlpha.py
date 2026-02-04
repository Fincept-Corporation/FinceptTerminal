# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-B44500AA
# Category: Alpha Model
# Description: Initialize helper variables for the algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

#
# A number of companies publicly trade two different classes of shares
# in US equity markets. If both assets trade with reasonable volume, then
# the underlying driving forces of each should be similar or the same. Given
# this, we can create a relatively dollar-neutral long/short portfolio using
# the dual share classes. Theoretically, any deviation of this portfolio from
# its mean-value should be corrected, and so the motivating idea is based on
# mean-reversion. Using a Simple Moving Average indicator, we can
# compare the value of this portfolio against its SMA and generate insights
# to buy the under-valued symbol and sell the over-valued symbol.
#
# This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open
# sourced so the community and client funds can see an example of an alpha.
#

class ShareClassMeanReversionAlpha(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2019, 1, 1)   #Set Start Date
        self.set_cash(100000)           #Set Strategy Cash
        self.set_warm_up(20)

        ## Setup Universe settings and tickers to be used
        tickers = ['VIA','VIAB']
        self.universe_settings.resolution = Resolution.MINUTE
        symbols = [ Symbol.create(ticker, SecurityType.EQUITY, Market.USA) for ticker in tickers]
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))  ## Set $0 fees to mimic High-Frequency Trading

        ## Set Manual Universe Selection
        self.set_universe_selection( ManualUniverseSelectionModel(symbols) )

        ## Set Custom Alpha Model
        self.set_alpha(ShareClassMeanReversionAlphaModel(tickers = tickers))

        ## Set Equal Weighting Portfolio Construction Model
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        ## Set Immediate Execution Model
        self.set_execution(ImmediateExecutionModel())

        ## Set Null Risk Management Model
        self.set_risk_management(NullRiskManagementModel())


class ShareClassMeanReversionAlphaModel(AlphaModel):
    ''' Initialize helper variables for the algorithm'''

    def __init__(self, *args, **kwargs):
        self.sma = SimpleMovingAverage(10)
        self.position_window = RollingWindow[float](2)
        self.alpha = None
        self.beta = None
        if 'tickers' not in kwargs:
            raise Exception('ShareClassMeanReversionAlphaModel: Missing argument: "tickers"')
        self.tickers = kwargs['tickers']
        self.position_value = None
        self.invested = False
        self.liquidate = 'liquidate'
        self.long_symbol = self.tickers[0]
        self.short_symbol = self.tickers[1]
        self.resolution = kwargs['resolution'] if 'resolution' in kwargs else Resolution.MINUTE
        self.prediction_interval = Time.multiply(Extensions.to_time_span(self.resolution), 5) ## Arbitrary
        self.insight_magnitude = 0.001

    def update(self, algorithm, data):
        insights = []

        ## Check to see if either ticker will return a NoneBar, and skip the data slice if so
        for security in algorithm.securities:
            if self.data_event_occured(data, security.key):
                return insights

        ## If Alpha and Beta haven't been calculated yet, then do so
        if (self.alpha is None) or (self.beta is None):
           self.calculate_alpha_beta(algorithm, data)
           algorithm.log('Alpha: ' + str(self.alpha))
           algorithm.log('Beta: ' + str(self.beta))

        ## If the SMA isn't fully warmed up, then perform an update
        if not self.sma.is_ready:
            self.update_indicators(data)
            return insights

        ## Update indicator and Rolling Window for each data slice passed into Update() method
        self.update_indicators(data)

        ## Check to see if the portfolio is invested. If no, then perform value comparisons and emit insights accordingly
        if not self.invested:
            if self.position_value >= self.sma.current.value:
                insights.append(Insight(self.long_symbol, self.prediction_interval, InsightType.PRICE, InsightDirection.DOWN, self.insight_magnitude, None))
                insights.append(Insight(self.short_symbol, self.prediction_interval, InsightType.PRICE, InsightDirection.UP, self.insight_magnitude, None))

                ## Reset invested boolean
                self.invested = True

            elif self.position_value < self.sma.current.value:
                insights.append(Insight(self.long_symbol, self.prediction_interval, InsightType.PRICE, InsightDirection.UP, self.insight_magnitude, None))
                insights.append(Insight(self.short_symbol, self.prediction_interval, InsightType.PRICE, InsightDirection.DOWN, self.insight_magnitude, None))

                ## Reset invested boolean
                self.invested = True

        ## If the portfolio is invested and crossed back over the SMA, then emit flat insights
        elif self.invested and self.crossed_mean():
            ## Reset invested boolean
            self.invested = False

        return Insight.group(insights)

    def data_event_occured(self, data, symbol):
        ## Helper function to check to see if data slice will contain a symbol
        if data.splits.contains_key(symbol) or \
           data.dividends.contains_key(symbol) or \
           data.delistings.contains_key(symbol) or \
           data.symbol_changed_events.contains_key(symbol):
            return True

    def update_indicators(self, data):
        ## Calculate position value and update the SMA indicator and Rolling Window
        self.position_value = (self.alpha * data[self.long_symbol].close) - (self.beta * data[self.short_symbol].close)
        self.sma.update(data[self.long_symbol].end_time, self.position_value)
        self.position_window.add(self.position_value)

    def crossed_mean(self):
        ## Check to see if the position value has crossed the SMA and then return a boolean value
        if (self.position_window[0] >= self.sma.current.value) and (self.position_window[1] < self.sma.current.value):
            return True
        elif (self.position_window[0] < self.sma.current.value) and (self.position_window[1] >= self.sma.current.value):
            return True
        else:
            return False

    def calculate_alpha_beta(self, algorithm, data):
        ## Calculate Alpha and Beta, the initial number of shares for each security needed to achieve a 50/50 weighting
        self.alpha = algorithm.calculate_order_quantity(self.long_symbol, 0.5)
        self.beta = algorithm.calculate_order_quantity(self.short_symbol, 0.5)
