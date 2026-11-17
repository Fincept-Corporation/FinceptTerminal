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
from Selection.ETFConstituentsUniverseSelectionModel import *

### <summary>
### Demonstration of using the ETFConstituentsUniverseSelectionModel
### </summary>
class ETFConstituentsFrameworkAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 12, 1)
        self.set_end_date(2020, 12, 7)
        self.set_cash(100000)

        self.universe_settings.resolution = Resolution.DAILY
        symbol = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        self.add_universe_selection(ETFConstituentsUniverseSelectionModel(symbol, self.universe_settings, self.etf_constituents_filter))

        self.add_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(days=1)))

        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())


    def etf_constituents_filter(self, constituents: List[ETFConstituentData]) -> List[Symbol]:
        # Get the 10 securities with the largest weight in the index
        selected = sorted([c for c in constituents if c.weight],
            key=lambda c: c.weight, reverse=True)[:8]
        return [c.symbol for c in selected]

