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

from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel
from Execution.ImmediateExecutionModel import ImmediateExecutionModel

### <summary>
### Basic template framework algorithm uses framework components to define the algorithm.
### Liquid ETF Competition template
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class LiquidETFUniverseFrameworkAlgorithm(QCAlgorithm):
    '''Basic template framework algorithm uses framework components to define the algorithm.'''

    def initialize(self):
        # Set Start Date so that backtest has 5+ years of data
        self.set_start_date(2014, 11, 1)
        # No need to set End Date as the final submission will be tested
        # up until the review date

        # Set $1m Strategy Cash to trade significant AUM
        self.set_cash(1000000)

        # Add a relevant benchmark, with the default being SPY
        self.set_benchmark('SPY')

        # Use the Alpha Streams Brokerage Model, developed in conjunction with
        # funds to model their actual fees, costs, etc.
        # Please do not add any additional reality modelling, such as Slippage, Fees, Buying Power, etc.
        self.set_brokerage_model(AlphaStreamsBrokerageModel())

        # Use the LiquidETFUniverse with minute-resolution data
        self.universe_settings.resolution = Resolution.MINUTE
        self.set_universe_selection(LiquidETFUniverse())

        # Optional
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

        # List of symbols we want to trade. Set it in OnSecuritiesChanged
        self._symbols = []

    def on_data(self, slice):

        if all([self.portfolio[x].invested for x in self._symbols]):
            return

        # Emit insights
        insights = [Insight.price(x, timedelta(1), InsightDirection.UP)
            for x in self._symbols if self.securities[x].price > 0]

        if len(insights) > 0:
            self.emit_insights(insights)

    def on_securities_changed(self, changes):

        # Set symbols as the Inverse Energy ETFs
        for security in changes.added_securities:
            if security.symbol in LiquidETFUniverse.ENERGY.inverse:
                self._symbols.append(security.symbol)

        # Print out the information about the groups
        self.log(f'Energy: {LiquidETFUniverse.ENERGY}')
        self.log(f'Metals: {LiquidETFUniverse.METALS}')
        self.log(f'Technology: {LiquidETFUniverse.TECHNOLOGY}')
        self.log(f'Treasuries: {LiquidETFUniverse.TREASURIES}')
        self.log(f'Volatility: {LiquidETFUniverse.VOLATILITY}')
        self.log(f'SP500Sectors: {LiquidETFUniverse.SP_500_SECTORS}')
