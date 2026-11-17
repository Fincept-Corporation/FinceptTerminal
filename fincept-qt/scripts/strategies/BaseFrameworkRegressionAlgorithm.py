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
### Abstract regression framework algorithm for multiple framework regression tests
### </summary>
class BaseFrameworkRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 6, 1)
        self.set_end_date(2014, 6, 30)

        self.universe_settings.resolution = Resolution.HOUR
        self.universe_settings.data_normalization_mode = DataNormalizationMode.RAW

        symbols = [Symbol.create(ticker, SecurityType.EQUITY, Market.USA)
            for ticker in ["AAPL", "AIG", "BAC", "SPY"]]

        # Manually add AAPL and AIG when the algorithm starts
        self.set_universe_selection(ManualUniverseSelectionModel(symbols[:2]))

        # At midnight, add all securities every day except on the last data
        # With this procedure, the Alpha Model will experience multiple universe changes
        self.add_universe_selection(ScheduledUniverseSelectionModel(
            self.date_rules.every_day(), self.time_rules.midnight,
            lambda dt: symbols if dt < self.end_date - timedelta(1) else []))

        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(31), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())

    def on_end_of_algorithm(self):
        # The base implementation checks for active insights
        insights_count = len(self.insights.get_insights(lambda insight: insight.is_active(self.utc_time)))
        if insights_count != 0:
            raise Exception(f"The number of active insights should be 0. Actual: {insights_count}")
