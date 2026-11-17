# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

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
