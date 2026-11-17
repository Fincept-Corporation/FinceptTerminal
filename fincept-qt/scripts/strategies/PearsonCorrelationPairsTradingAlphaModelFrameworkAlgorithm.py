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
from Alphas.PearsonCorrelationPairsTradingAlphaModel import PearsonCorrelationPairsTradingAlphaModel

### <summary>
### Framework algorithm that uses the PearsonCorrelationPairsTradingAlphaModel.
### This model extendes BasePairsTradingAlphaModel and uses Pearson correlation
### to rank the pairs trading candidates and use the best candidate to trade.
### </summary>
class PearsonCorrelationPairsTradingAlphaModelFrameworkAlgorithm(QCAlgorithm):
    '''Framework algorithm that uses the PearsonCorrelationPairsTradingAlphaModel.
    This model extendes BasePairsTradingAlphaModel and uses Pearson correlation
    to rank the pairs trading candidates and use the best candidate to trade.'''

    def initialize(self):

        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)

        symbols = [Symbol.create(ticker, SecurityType.EQUITY, Market.USA)
            for ticker in ["SPY", "AIG", "BAC", "IBM"]]

        # Manually add SPY and AIG when the algorithm starts
        self.set_universe_selection(ManualUniverseSelectionModel(symbols[:2]))

        # At midnight, add all securities every day except on the last data
        # With this procedure, the Alpha Model will experience multiple universe changes
        self.add_universe_selection(ScheduledUniverseSelectionModel(
            self.date_rules.every_day(), self.time_rules.midnight,
            lambda dt: symbols if dt.day <= (self.end_date - timedelta(1)).day else []))

        self.set_alpha(PearsonCorrelationPairsTradingAlphaModel(252, Resolution.DAILY))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())

    def on_end_of_algorithm(self) -> None:
        # We have removed all securities from the universe. The Alpha Model should remove the consolidator
        consolidator_count = sum(s.consolidators.count for s in self.subscription_manager.subscriptions)
        if consolidator_count > 0:
            raise Exception(f"The number of consolidator should be zero. Actual: {consolidator_count}")
