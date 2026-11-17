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
### Show cases how to use the CompositeRiskManagementModel.
### </summary>
class CompositeRiskManagementModelFrameworkAlgorithm(QCAlgorithm):
    '''Show cases how to use the CompositeRiskManagementModel.'''

    def initialize(self):

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel([Symbol.create("SPY", SecurityType.EQUITY, Market.USA)]))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

        # define risk management model as a composite of several risk management models
        self.set_risk_management(CompositeRiskManagementModel(
            MaximumUnrealizedProfitPercentPerSecurity(0.01),
            MaximumDrawdownPercentPerSecurity(0.01)
        ))
