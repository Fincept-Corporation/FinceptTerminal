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
### Regression algorithm testing GH feature 3790, using SetHoldings with a collection of targets
### which will be ordered by margin impact before being executed, with the objective of avoiding any
### margin errors
### </summary>
class SetHoldingsMultipleTargetsRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10,11)

        # use leverage 1 so we test the margin impact ordering
        self._spy = self.add_equity("SPY", Resolution.MINUTE, Market.USA, False, 1).symbol
        self._ibm = self.add_equity("IBM", Resolution.MINUTE, Market.USA, False, 1).symbol

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings([PortfolioTarget(self._spy, 0.8), PortfolioTarget(self._ibm, 0.2)])
        else:
            self.set_holdings([PortfolioTarget(self._ibm, 0.8), PortfolioTarget(self._spy, 0.2)])
