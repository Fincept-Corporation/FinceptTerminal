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
from Orders.Slippage.VolumeShareSlippageModel import VolumeShareSlippageModel

### <summary>
### Example algorithm implementing VolumeShareSlippageModel.
### </summary>
class VolumeShareSlippageModelAlgorithm(QCAlgorithm):
    longs = []
    shorts = []

    def initialize(self) -> None:
        self.set_start_date(2020, 11, 29)
        self.set_end_date(2020, 12, 2)
        # To set the slippage model to limit to fill only 30% volume of the historical volume, with 5% slippage impact.
        self.set_security_initializer(lambda security: security.set_slippage_model(VolumeShareSlippageModel(0.3, 0.05)))

        # Create SPY symbol to explore its constituents.
        spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        
        self.universe_settings.resolution = Resolution.DAILY
        # Add universe to trade on the most and least weighted stocks among SPY constituents.
        self.add_universe(self.universe.etf(spy, universe_filter_func=self.selection))

    def selection(self, constituents: List[ETFConstituentUniverse]) -> List[Symbol]:
        sorted_by_weight = sorted(constituents, key=lambda c: c.weight)
        # Add the 10 most weighted stocks to the universe to long later.
        self.longs = [c.symbol for c in sorted_by_weight[-10:]]
        # Add the 10 least weighted stocks to the universe to short later.
        self.shorts = [c.symbol for c in sorted_by_weight[:10]]

        return self.longs + self.shorts

    def on_data(self, slice: Slice) -> None:
        # Equally invest into the selected stocks to evenly dissipate capital risk.
        # Dollar neutral of long and short stocks to eliminate systematic risk, only capitalize the popularity gap.
        targets = [PortfolioTarget(symbol, 0.05) for symbol in self.longs]
        targets += [PortfolioTarget(symbol, -0.05) for symbol in self.shorts]

        # Liquidate the ones not being the most and least popularity stocks to release fund for higher expected return trades.
        self.set_holdings(targets, liquidate_existing_holdings=True)
