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
from Portfolio.RiskParityPortfolioConstructionModel import *

class RiskParityPortfolioAlgorithm(QCAlgorithm):
    '''Example algorithm of using RiskParityPortfolioConstructionModel'''

    def initialize(self):
        self.set_start_date(2021, 2, 21)  # Set Start Date
        self.set_end_date(2021, 3, 30)
        self.set_cash(100000)  # Set Strategy Cash
        self.set_security_initializer(lambda security: security.set_market_price(self.get_last_known_price(security)))

        self.add_equity("SPY", Resolution.DAILY)
        self.add_equity("AAPL", Resolution.DAILY)

        self.add_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1)))
        self.set_portfolio_construction(RiskParityPortfolioConstructionModel())
