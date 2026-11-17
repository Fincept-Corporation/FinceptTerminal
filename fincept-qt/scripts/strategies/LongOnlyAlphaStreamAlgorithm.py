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
from Selection.ManualUniverseSelectionModel import ManualUniverseSelectionModel

### <summary>
### Basic template framework algorithm uses framework components to define the algorithm.
### Shows EqualWeightingPortfolioConstructionModel.long_only() application
### </summary>
### <meta name="tag" content="alpha streams" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="algorithm framework" />
class LongOnlyAlphaStreamAlgorithm(QCAlgorithm):

    def initialize(self):
        # 1. Required:
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

        # 2. Required: Alpha Streams Models:
        self.set_brokerage_model(BrokerageName.ALPHA_STREAMS)

        # 3. Required: Significant AUM Capacity
        self.set_cash(1000000)

        # Only SPY will be traded
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(Resolution.DAILY, PortfolioBias.LONG))
        self.set_execution(ImmediateExecutionModel())

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

        # Set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel(
            [Symbol.create(x, SecurityType.EQUITY, Market.USA) for x in ["SPY", "IBM"]]))

    def on_data(self, slice):
        if self.portfolio.invested: return

        self.emit_insights(
            [
                Insight.price("SPY", timedelta(1), InsightDirection.UP),
                Insight.price("IBM", timedelta(1), InsightDirection.DOWN)
            ])

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            if self.securities[order_event.symbol].holdings.is_short:
                raise ValueError("Invalid position, should not be short")
            self.debug(order_event)
