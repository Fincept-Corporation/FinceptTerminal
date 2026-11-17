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

### <summary>
### Alpha Benchmark Strategy capitalizing on ETF rebalancing causing momentum during trending markets.
### </summary>
### <meta name="tag" content="alphastream" />
### <meta name="tag" content="etf" />
### <meta name="tag" content="algorithm framework" />
class RebalancingLeveragedETFAlpha(QCAlgorithm):
    ''' Alpha Streams: Benchmark Alpha: Leveraged ETF Rebalancing
        Strategy by Prof. Shum, reposted by Ernie Chan.
        Source: http://epchan.blogspot.com/2012/10/a-leveraged-etfs-strategy.html'''

    def initialize(self):

        self.set_start_date(2017, 6, 1)
        self.set_end_date(2018, 8, 1)
        self.set_cash(100000)

        underlying = ["SPY","QLD","DIA","IJR","MDY","IWM","QQQ","IYE","EEM","IYW","EFA","GAZB","SLV","IEF","IYM","IYF","IYH","IYR","IYC","IBB","FEZ","USO","TLT"]
        ultra_long =  ["SSO","UGL","DDM","SAA","MZZ","UWM","QLD","DIG","EET","ROM","EFO","BOIL","AGQ","UST","UYM","UYG","RXL","URE","UCC","BIB","ULE","UCO","UBT"]
        ultra_short = ["SDS","GLL","DXD","SDD","MVV","TWM","QID","DUG","EEV","REW","EFU","KOLD","ZSL","PST","SMN","SKF","RXD","SRS","SCC","BIS","EPV","SCO","TBT"]

        groups = []
        for i in range(len(underlying)):
            group = ETFGroup(self.add_equity(underlying[i], Resolution.MINUTE).symbol,
                              self.add_equity(ultra_long[i], Resolution.MINUTE).symbol,
                              self.add_equity(ultra_short[i], Resolution.MINUTE).symbol)
            groups.append(group)

        # Manually curated universe
        self.set_universe_selection(ManualUniverseSelectionModel())
        # Select the demonstration alpha model
        self.set_alpha(RebalancingLeveragedETFAlphaModel(groups))

        # Equally weigh securities in portfolio, based on insights
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        # Set Immediate Execution Model
        self.set_execution(ImmediateExecutionModel())

        # Set Null Risk Management Model
        self.set_risk_management(NullRiskManagementModel())


class RebalancingLeveragedETFAlphaModel(AlphaModel):

    '''
        If the underlying ETF has experienced a return >= 1% since the previous day's close up to the current time at 14:15,
        then buy it's ultra ETF right away, and exit at the close. If the return is <= -1%, sell it's ultra-short ETF.
    '''

    def __init__(self, ETFgroups):

        self.etfgroups = ETFgroups
        self.date = datetime.min.date
        self.name = "RebalancingLeveragedETFAlphaModel"

    def update(self, algorithm, data):
        '''Scan to see if the returns are greater than 1% at 2.15pm to emit an insight.'''

        insights = []
        magnitude = 0.0005
        # Paper suggests leveraged ETF's rebalance from 2.15pm - to close
        # giving an insight period of 105 minutes.
        period = timedelta(minutes=105)

        # Get yesterday's close price at the market open
        if algorithm.time.date() != self.date:
            self.date = algorithm.time.date()
            # Save yesterday's price and reset the signal
            for group in self.etfgroups:
                history = algorithm.history([group.underlying], 1, Resolution.DAILY)
                group.yesterday_close = None if history.empty else history.loc[str(group.underlying)]['close'][0]

        # Check if the returns are > 1% at 14.15
        if algorithm.time.hour == 14 and algorithm.time.minute == 15:
            for group in self.etfgroups:
                if group.yesterday_close == 0 or group.yesterday_close is None: continue
                returns = round((algorithm.portfolio[group.underlying].price - group.yesterday_close) / group.yesterday_close, 10)
                if returns > 0.01:
                    insights.append(Insight.price(group.ultra_long, period, InsightDirection.UP, magnitude))
                elif returns < -0.01:
                    insights.append(Insight.price(group.ultra_short, period, InsightDirection.DOWN, magnitude))

        return insights

class ETFGroup:
    '''
    Group the underlying ETF and it's ultra ETFs
    Args:
        underlying: The underlying index ETF
        ultra_long: The long-leveraged version of underlying ETF
        ultra_short: The short-leveraged version of the underlying ETF
    '''
    def __init__(self,underlying, ultra_long, ultra_short):
        self.underlying = underlying
        self.ultra_long = ultra_long
        self.ultra_short = ultra_short
        self.yesterday_close = 0
