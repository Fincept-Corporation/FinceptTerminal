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
from Alphas.ConstantAlphaModel import ConstantAlphaModel
from Selection.EmaCrossUniverseSelectionModel import EmaCrossUniverseSelectionModel
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel

### <summary>
### Framework algorithm that uses the EmaCrossUniverseSelectionModel to
### select the universe based on a moving average cross.
### </summary>
class EmaCrossUniverseSelectionFrameworkAlgorithm(QCAlgorithm):
    '''Framework algorithm that uses the EmaCrossUniverseSelectionModel to select the universe based on a moving average cross.'''

    def initialize(self):

        self.set_start_date(2013,1,1)
        self.set_end_date(2015,1,1)
        self.set_cash(100000)

        fast_period = 100
        slow_period = 300
        count = 10

        self.universe_settings.leverage = 2.0
        self.universe_settings.resolution = Resolution.DAILY

        self.set_universe_selection(EmaCrossUniverseSelectionModel(fast_period, slow_period, count))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1), None, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
