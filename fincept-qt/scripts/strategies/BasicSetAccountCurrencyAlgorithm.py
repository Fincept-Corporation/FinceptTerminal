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
### Basic algorithm using SetAccountCurrency
### </summary>
class BasicSetAccountCurrencyAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2018, 4, 4)  #Set Start Date
        self.set_end_date(2018, 4, 4)    #Set End Date
        self.set_brokerage_model(BrokerageName.GDAX, AccountType.CASH)
        self.set_account_currency_and_amount()

        self._btc_eur = self.add_crypto("BTCEUR").symbol

    def set_account_currency_and_amount(self):
        # Before setting any cash or adding a Security call SetAccountCurrency
        self.set_account_currency("EUR")
        self.set_cash(100000)           #Set Strategy Cash

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings(self._btc_eur, 1)
