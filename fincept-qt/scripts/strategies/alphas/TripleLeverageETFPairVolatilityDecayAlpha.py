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
# Leveraged ETFs (LETF) promise a fixed leverage ratio with respect to an underlying asset or an index.
# A Triple-Leveraged ETF allows speculators to amplify their exposure to the daily returns of an underlying index by a factor of 3.
#
# Increased volatility generally decreases the value of a LETF over an extended period of time as daily compounding is amplified.
#
# This alpha emits short-biased insight to capitalize on volatility decay for each listed pair of TL-ETFs, by rebalancing the
# ETFs with equal weights each day.
#
# This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open sourced so the community and client funds can see an example of an alpha.
#

class TripleLeverageETFPairVolatilityDecayAlpha(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2018, 1, 1)

        self.set_cash(100000)

        # Set zero transaction fees
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))

        # 3X ETF pair tickers
        ultra_long = Symbol.create("UGLD", SecurityType.EQUITY, Market.USA)
        ultra_short = Symbol.create("DGLD", SecurityType.EQUITY, Market.USA)

        # Manually curated universe
        self.universe_settings.resolution = Resolution.DAILY
        self.set_universe_selection(ManualUniverseSelectionModel([ultra_long, ultra_short]))

        # Select the demonstration alpha model
        self.set_alpha(RebalancingTripleLeveragedETFAlphaModel(ultra_long, ultra_short))

        ## Set Equal Weighting Portfolio Construction Model
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        ## Set Immediate Execution Model
        self.set_execution(ImmediateExecutionModel())

        ## Set Null Risk Management Model
        self.set_risk_management(NullRiskManagementModel())


class RebalancingTripleLeveragedETFAlphaModel(AlphaModel):
    '''
        Rebalance a pair of 3x leveraged ETFs and predict that the value of both ETFs in each pair will decrease.
    '''

    def __init__(self, ultra_long, ultra_short):
        # Giving an insight period 1 days.
        self.period = timedelta(1)

        self.magnitude = 0.001
        self.ultra_long = ultra_long
        self.ultra_short = ultra_short

        self.name = "RebalancingTripleLeveragedETFAlphaModel"

    def update(self, algorithm, data):

        return Insight.group(
            [
                Insight.price(self.ultra_long, self.period, InsightDirection.DOWN, self.magnitude),
                Insight.price(self.ultra_short, self.period, InsightDirection.DOWN, self.magnitude)
            ] )
