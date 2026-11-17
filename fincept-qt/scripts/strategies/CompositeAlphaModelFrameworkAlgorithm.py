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

from Alphas.RsiAlphaModel import RsiAlphaModel
from Alphas.EmaCrossAlphaModel import EmaCrossAlphaModel
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel

### <summary>
### Show cases how to use the CompositeAlphaModel to define.
### </summary>
class CompositeAlphaModelFrameworkAlgorithm(QCAlgorithm):
    '''Show cases how to use the CompositeAlphaModel to define.'''

    def initialize(self):
        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        # even though we're using a framework algorithm, we can still add our securities
        # using the AddEquity/Forex/Crypto/ect methods and then pass them into a manual
        # universe selection model using securities.keys()
        self.add_equity("SPY")
        self.add_equity("IBM")
        self.add_equity("BAC")
        self.add_equity("AIG")

        # define a manual universe of all the securities we manually registered
        self.set_universe_selection(ManualUniverseSelectionModel())

        # define alpha model as a composite of the rsi and ema cross models
        self.set_alpha(CompositeAlphaModel(RsiAlphaModel(), EmaCrossAlphaModel()))

        # default models for the rest
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())
