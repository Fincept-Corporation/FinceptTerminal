# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-EEAA6B87
# Category: Alpha Model
# Description: Alpha Streams: Benchmark Alpha: Identify "pumped" penny stocks and predict that the price of a "pumped" penny stock r...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Selection.FundamentalUniverseSelectionModel import FundamentalUniverseSelectionModel

class SykesShortMicroCapAlpha(QCAlgorithm):
    ''' Alpha Streams: Benchmark Alpha: Identify "pumped" penny stocks and predict that the price of a "pumped" penny stock reverts to mean

    This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open
   sourced so the community and client funds can see an example of an alpha.'''

    def initialize(self):

        self.set_start_date(2018, 1, 1)
        self.set_cash(100000)

        # Set zero transaction fees
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))

        # select stocks using PennyStockUniverseSelectionModel
        self.universe_settings.resolution = Resolution.DAILY
        self.universe_settings.schedule.on(self.date_rules.month_start())
        self.set_universe_selection(PennyStockUniverseSelectionModel())

        # Use SykesShortMicroCapAlphaModel to establish insights
        self.set_alpha(SykesShortMicroCapAlphaModel())

        # Equally weigh securities in portfolio, based on insights
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        # Set Immediate Execution Model
        self.set_execution(ImmediateExecutionModel())

        # Set Null Risk Management Model
        self.set_risk_management(NullRiskManagementModel())


class SykesShortMicroCapAlphaModel(AlphaModel):
    '''Uses ranking of intraday percentage difference between open price and close price to create magnitude and direction prediction for insights'''

    def __init__(self, *args, **kwargs):
        lookback = kwargs['lookback'] if 'lookback' in kwargs else 1
        resolution = kwargs['resolution'] if 'resolution' in kwargs else Resolution.DAILY
        self.prediction_interval = Time.multiply(Extensions.to_time_span(resolution), lookback)
        self.number_of_stocks = kwargs['number_of_stocks'] if 'number_of_stocks' in kwargs else 10

    def update(self, algorithm, data):
        insights = []
        symbols_ret = dict()

        for security in algorithm.active_securities.values:
            if security.has_data:
                open_ = security.open
                if open_ != 0:
                    # Intraday price change for penny stocks
                    symbols_ret[security.symbol] = security.close / open_ - 1

        # Rank penny stocks on one day price change and retrieve list of ten "pumped" penny stocks
        pumped_stocks = dict(sorted(symbols_ret.items(),
                                   key = lambda kv: (-round(kv[1], 6), kv[0]))[:self.number_of_stocks])

        # Emit "down" insight for "pumped" penny stocks
        for symbol, value in pumped_stocks.items():
            insights.append(Insight.price(symbol, self.prediction_interval, InsightDirection.DOWN, abs(value), None))

        return insights


class PennyStockUniverseSelectionModel(FundamentalUniverseSelectionModel):
    '''Defines a universe of penny stocks, as a universe selection model for the framework algorithm:
    The stocks must have fundamental data
    The stock must have positive previous-day close price
    The stock must have volume between $1000000 and $10000 on the previous trading day
    The stock must cost less than $5'''
    def __init__(self):
        super().__init__()

        # Number of stocks in Coarse Universe
        self.number_of_symbols_coarse = 500

    def select(self, algorithm, fundamental):
        # sort the stocks by dollar volume and take the top 500
        top = sorted([x for x in fundamental if x.has_fundamental_data
                                       and 5 > x.price > 0
                                       and 1000000 > x.volume > 10000],
                    key=lambda x: x.dollar_volume, reverse=True)[:self.number_of_symbols_coarse]

        return [x.symbol for x in top]
