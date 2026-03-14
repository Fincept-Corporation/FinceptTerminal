# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-217A291D
# Category: Alpha Model
# Description: Triangle Exchange Rate Arbitrage Alpha
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

#
# In a perfect market, you could buy 100 EUR worth of USD, sell 100 EUR worth of GBP,
# and then use the GBP to buy USD and wind up with the same amount in USD as you received when
# you bought them with EUR. This relationship is expressed by the Triangle Exchange Rate, which is
#
#     Triangle Exchange Rate = (A/B) * (B/C) * (C/A)
#
# where (A/B) is the exchange rate of A-to-B. In a perfect market, TER = 1, and so when
# there is a mispricing in the market, then TER will not be 1 and there exists an arbitrage opportunity.
#
# This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open sourced so the community and client funds can see an example of an alpha.
#

class TriangleExchangeRateArbitrageAlpha(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2019, 2, 1)   #Set Start Date
        self.set_cash(100000)           #Set Strategy Cash

        # Set zero transaction fees
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))

        ## Select trio of currencies to trade where
        ## Currency A = USD
        ## Currency B = EUR
        ## Currency C = GBP
        currencies = ['EURUSD','EURGBP','GBPUSD']
        symbols = [ Symbol.create(currency, SecurityType.FOREX, Market.OANDA) for currency in currencies]

        ## Manual universe selection with tick-resolution data
        self.universe_settings.resolution = Resolution.MINUTE
        self.set_universe_selection( ManualUniverseSelectionModel(symbols) )

        self.set_alpha(ForexTriangleArbitrageAlphaModel(Resolution.MINUTE, symbols))

        ## Set Equal Weighting Portfolio Construction Model
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        ## Set Immediate Execution Model
        self.set_execution(ImmediateExecutionModel())

        ## Set Null Risk Management Model
        self.set_risk_management(NullRiskManagementModel())


class ForexTriangleArbitrageAlphaModel(AlphaModel):

    def __init__(self, insight_resolution, symbols):
        self.insight_period = Time.multiply(Extensions.to_time_span(insight_resolution), 5)
        self._symbols = symbols

    def update(self, algorithm, data):
        ## Check to make sure all currency symbols are present
        if len(data.keys()) < 3:
            return []

        ## Extract QuoteBars for all three Forex securities
        bar_a = data[self._symbols[0]]
        bar_b = data[self._symbols[1]]
        bar_c = data[self._symbols[2]]

        ## Calculate the triangle exchange rate
        ## Bid(Currency A -> Currency B) * Bid(Currency B -> Currency C) * Bid(Currency C -> Currency A)
        ## If exchange rates are priced perfectly, then this yield 1. If it is different than 1, then an arbitrage opportunity exists
        triangle_rate = bar_a.ask.close / bar_b.bid.close / bar_c.ask.close

        ## If the triangle rate is significantly different than 1, then emit insights
        if triangle_rate > 1.0005:
            return Insight.group(
                [
                    Insight.price(self._symbols[0], self.insight_period, InsightDirection.UP, 0.0001, None),
                    Insight.price(self._symbols[1], self.insight_period, InsightDirection.DOWN, 0.0001, None),
                    Insight.price(self._symbols[2], self.insight_period, InsightDirection.UP, 0.0001, None)
                ] )

        return []
