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
from Portfolio.MeanReversionPortfolioConstructionModel import *

class MeanReversionPortfolioAlgorithm(QCAlgorithm):
    '''Example algorithm of using MeanReversionPortfolioConstructionModel'''

    def initialize(self):
        # Set starting date, cash and ending date of the backtest
        self.set_start_date(2020, 9, 1)
        self.set_end_date(2021, 2, 28)
        self.set_cash(100000)

        self.set_security_initializer(lambda security: security.set_market_price(self.get_last_known_price(security)))
        
        # Subscribe to data of the selected stocks
        self._symbols = [self.add_equity(ticker, Resolution.DAILY).symbol for ticker in ["SPY", "AAPL"]]

        self.add_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1)))
        self.set_portfolio_construction(MeanReversionPortfolioConstructionModel())
