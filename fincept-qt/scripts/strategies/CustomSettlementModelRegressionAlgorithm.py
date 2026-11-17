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
from CustomBrokerageModelRegressionAlgorithm import CustomBrokerageModel

### <summary>
### Regression algorithm to test we can specify a custom settlement model, and override some of its methods
### </summary>
class CustomSettlementModelRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)
        self.set_cash(10000)
        self.spy = self.add_equity("SPY", Resolution.DAILY)
        self.set_settlement_model(self.spy)

    def set_settlement_model(self, security):
        self.set_brokerage_model(CustomBrokerageModelWithCustomSettlementModel())

    def on_data(self, slice):
        if self.portfolio.cash_book[Currencies.USD].amount == 10000:
            parameters = ApplyFundsSettlementModelParameters(self.portfolio, self.spy, self.time, CashAmount(101, Currencies.USD), None)
            self.spy.settlement_model.apply_funds(parameters)

    def on_end_of_algorithm(self):
        if self.portfolio.cash_book[Currencies.USD].amount != 10101:
            raise Exception(f"It was expected to have 10101 USD in Portfolio, but was {self.portfolio.cash_book[Currencies.USD].amount}")
        parameters = ScanSettlementModelParameters(self.portfolio, self.spy, datetime(2013, 10, 6))
        self.spy.settlement_model.scan(parameters)
        if self.portfolio.cash_book[Currencies.USD].amount != 10000:
            raise Exception(f"It was expected to have 10000 USD in Portfolio, but was {self.portfolio.cash_book[Currencies.USD].amount}")

class CustomSettlementModel:
    def apply_funds(self, parameters):
        self.currency = parameters.cash_amount.currency
        self.amount = parameters.cash_amount.amount
        parameters.portfolio.cash_book[self.currency].add_amount(self.amount)

    def scan(self, parameters):
        if parameters.utc_time == datetime(2013, 10, 6):
            parameters.portfolio.cash_book[self.currency].add_amount(-self.amount)

    def get_unsettled_cash(self):
        return None

class CustomBrokerageModelWithCustomSettlementModel(CustomBrokerageModel):
    def get_settlement_model(self, security):
        return CustomSettlementModel()
