# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License")
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
### Demonstration of using custom margin interest rate model in backtesting.
### </summary>
class CustomMarginInterestRateModelAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 1)
        self.set_end_date(2013, 10, 31)

        security = self.add_equity("SPY", Resolution.HOUR)
        self._spy = security.symbol

        # set the margin interest rate model
        self._margin_interest_rate_model = CustomMarginInterestRateModel()
        security.set_margin_interest_rate_model(self._margin_interest_rate_model)

        self._cash_after_order = 0

    def on_data(self, data: Slice):
        if not self.portfolio.invested:
            self.set_holdings(self._spy, 1)

    def on_order_event(self, order_event: OrderEvent):
        if order_event.status == OrderStatus.FILLED:
            self._cash_after_order = self.portfolio.cash

    def on_end_of_algorithm(self):
        if self._margin_interest_rate_model.call_count == 0:
            raise Exception("CustomMarginInterestRateModel was not called")

        expected_cash = self._cash_after_order * pow(1 + self._margin_interest_rate_model.interest_rate, self._margin_interest_rate_model.call_count)

        if abs(self.portfolio.cash - expected_cash) > 1e-10:
            raise Exception(f"Expected cash {expected_cash} but got {self.portfolio.cash}")


class CustomMarginInterestRateModel:
    def __init__(self):
        self.interest_rate = 0.01
        self.call_count = 0

    def apply_margin_interest_rate(self, parameters: MarginInterestRateParameters):
        security = parameters.security
        position_value = security.holdings.get_quantity_value(security.holdings.quantity)

        if position_value.amount > 0:
            position_value.cash.add_amount(self.interest_rate * position_value.cash.amount)
            self.call_count += 1
