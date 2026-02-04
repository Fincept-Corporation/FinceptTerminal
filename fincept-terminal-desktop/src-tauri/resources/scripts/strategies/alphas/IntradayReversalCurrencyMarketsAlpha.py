# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5596A325
# Category: Alpha Model
# Description: Alpha model that uses a Price/SMA Crossover to create insights on Hourly Frequency.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

#
# Reversal strategy that goes long when price crosses below SMA and Short when price crosses above SMA.
# The trading strategy is implemented only between 10AM - 3PM (NY time). Research suggests this is due to
# institutional trades during market hours which need hedging with the USD. Source paper:
# LeBaron, Zhao: Intraday Foreign Exchange Reversals
# http://people.brandeis.edu/~blebaron/wps/fxnyc.pdf
# http://www.fma.org/Reno/Papers/ForeignExchangeReversalsinNewYorkTime.PDF
#
# This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open sourced so the community and client funds can see an example of an alpha.
#

class IntradayReversalCurrencyMarketsAlpha(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2015, 1, 1)
        self.set_cash(100000)

        # Set zero transaction fees
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))

        # Select resolution
        resolution = Resolution.HOUR

        # Reversion on the USD.
        symbols = [Symbol.create("EURUSD", SecurityType.FOREX, Market.OANDA)]

        # Set requested data resolution
        self.universe_settings.resolution = resolution
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))
        self.set_alpha(IntradayReversalAlphaModel(5, resolution))

        # Equally weigh securities in portfolio, based on insights
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        # Set Immediate Execution Model
        self.set_execution(ImmediateExecutionModel())

        # Set Null Risk Management Model
        self.set_risk_management(NullRiskManagementModel())

        #Set WarmUp for Indicators
        self.set_warm_up(20)


class IntradayReversalAlphaModel(AlphaModel):
    '''Alpha model that uses a Price/SMA Crossover to create insights on Hourly Frequency.
    Frequency: Hourly data with 5-hour simple moving average.
    Strategy:
    Reversal strategy that goes Long when price crosses below SMA and Short when price crosses above SMA.
    The trading strategy is implemented only between 10AM - 3PM (NY time)'''

    # Initialize variables
    def __init__(self, period_sma = 5, resolution = Resolution.HOUR):
        self.period_sma = period_sma
        self.resolution = resolution
        self.cache = {} # Cache for SymbolData
        self.name = 'IntradayReversalAlphaModel'

    def update(self, algorithm, data):
        # Set the time to close all positions at 3PM
        time_to_close = algorithm.time.replace(hour=15, minute=1, second=0)

        insights = []
        for kvp in algorithm.active_securities:

            symbol = kvp.key

            if self.should_emit_insight(algorithm, symbol) and symbol in self.cache:

                price = kvp.value.price
                symbol_data = self.cache[symbol]

                direction = InsightDirection.UP if symbol_data.is_uptrend(price) else InsightDirection.DOWN

                # Ignore signal for same direction as previous signal (when no crossover)
                if direction == symbol_data.previous_direction:
                    continue

                # Save the current Insight Direction to check when the crossover happens
                symbol_data.previous_direction = direction

                # Generate insight
                insights.append(Insight.price(symbol, time_to_close, direction))

        return insights

    def on_securities_changed(self, algorithm, changes):
        '''Handle creation of the new security and its cache class.
        Simplified in this example as there is 1 asset.'''
        for security in changes.added_securities:
            self.cache[security.symbol] = SymbolData(algorithm, security.symbol, self.period_sma, self.resolution)

    def should_emit_insight(self, algorithm, symbol):
        '''Time to control when to start and finish emitting (10AM to 3PM)'''
        time_of_day = algorithm.time.time()
        return algorithm.securities[symbol].has_data and time_of_day >= time(10) and time_of_day <= time(15)


class SymbolData:

    def __init__(self, algorithm, symbol, period_sma, resolution):
        self.previous_direction = InsightDirection.FLAT
        self.price_sma = algorithm.sma(symbol, period_sma, resolution)

    def is_uptrend(self, price):
        return self.price_sma.is_ready and price < round(self.price_sma.current.value * 1.001, 6)
