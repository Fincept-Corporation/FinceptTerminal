# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C3B2CF90
# Category: Alpha Model
# Description: Uses ranking of Internal Bar Strength (IBS) to create direction prediction for insights
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

#
# Equity indices exhibit mean reversion in daily returns. The Internal Bar Strength indicator (IBS),
# which relates the closing price of a security to its daily range can be used to identify overbought
# and oversold securities.
#
# This alpha ranks 33 global equity ETFs on its IBS value the previous day and predicts for the following day
# that the ETF with the highest IBS value will decrease in price, and the ETF with the lowest IBS value
# will increase in price.
#
# Source: Kakushadze, Zura, and Juan Andrés Serur. “4. Exchange-Traded Funds (ETFs).” 151 Trading Strategies, Palgrave Macmillan, 2018, pp. 90–91.
#
# This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open sourced so the community and client funds can see an example of an alpha.
#

class GlobalEquityMeanReversionIBSAlpha(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2018, 1, 1)

        self.set_cash(100000)

        # Set zero transaction fees
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))

        # Global Equity ETF tickers
        tickers = ["ECH","EEM","EFA","EPHE","EPP","EWA","EWC","EWG",
                   "EWH","EWI","EWJ","EWL","EWM","EWM","EWO","EWP",
                   "EWQ","EWS","EWT","EWU","EWY","EWZ","EZA","FXI",
                   "GXG","IDX","ILF","EWM","QQQ","RSX","SPY","THD"]

        symbols = [Symbol.create(ticker, SecurityType.EQUITY, Market.USA) for ticker in tickers]

        # Manually curated universe
        self.universe_settings.resolution = Resolution.DAILY
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))

        # Use GlobalEquityMeanReversionAlphaModel to establish insights
        self.set_alpha(MeanReversionIBSAlphaModel())

        # Equally weigh securities in portfolio, based on insights
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        # Set Immediate Execution Model
        self.set_execution(ImmediateExecutionModel())

        # Set Null Risk Management Model
        self.set_risk_management(NullRiskManagementModel())


class MeanReversionIBSAlphaModel(AlphaModel):
    '''Uses ranking of Internal Bar Strength (IBS) to create direction prediction for insights'''

    def __init__(self, *args, **kwargs):
        lookback = kwargs['lookback'] if 'lookback' in kwargs else 1
        resolution = kwargs['resolution'] if 'resolution' in kwargs else Resolution.DAILY
        self.prediction_interval = Time.multiply(Extensions.to_time_span(resolution), lookback)
        self.number_of_stocks = kwargs['number_of_stocks'] if 'number_of_stocks' in kwargs else 2

    def update(self, algorithm, data):

        insights = []
        symbols_ibs = dict()
        returns = dict()

        for security in algorithm.active_securities.values:
            if security.has_data:
                high = security.high
                low = security.low
                hilo = high - low

                # Do not consider symbol with zero open and avoid division by zero
                if security.open * hilo != 0:
                    # Internal bar strength (IBS)
                    symbols_ibs[security.symbol] = (security.close - low)/hilo
                    returns[security.symbol] = security.close/security.open-1

        # Number of stocks cannot be higher than half of symbols_ibs length
        number_of_stocks = min(int(len(symbols_ibs)/2), self.number_of_stocks)
        if number_of_stocks == 0:
            return []

        # Rank securities with the highest IBS value
        ordered = sorted(symbols_ibs.items(), key=lambda kv: (round(kv[1], 6), kv[0]), reverse=True)
        high_ibs = dict(ordered[0:number_of_stocks])   # Get highest IBS
        low_ibs = dict(ordered[-number_of_stocks:])    # Get lowest IBS

        # Emit "down" insight for the securities with the highest IBS value
        for key,value in high_ibs.items():
            insights.append(Insight.price(key, self.prediction_interval, InsightDirection.DOWN, abs(returns[key]), None))

        # Emit "up" insight for the securities with the lowest IBS value
        for key,value in low_ibs.items():
            insights.append(Insight.price(key, self.prediction_interval, InsightDirection.UP, abs(returns[key]), None))

        return insights
