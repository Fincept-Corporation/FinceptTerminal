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
### Asserts we can use a Python lambda function as a FuncRiskFreeRateInterestRateModel
### </summary>
class FuncRiskFreeRateInterestRateModelWithPythonLambda(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2020, 5, 28)
        self.set_end_date(2020, 6, 28)

        self.add_equity("SPY", Resolution.DAILY)
        self.model = FuncRiskFreeRateInterestRateModel(lambda dt: 1 if dt.date != datetime(2020, 5, 28) else 0)

    def on_data(self, slice):
        if self.time.date == datetime(2020, 5, 28) and self.model.get_interest_rate(self.time) != 0:
            raise Exception(f"Risk free interest rate should be 0, but was {self.model.get_interest_rate(self.time)}")
        elif self.time.date != datetime(2020, 5, 28) and self.model.get_interest_rate(self.time) != 1:
            raise Exception(f"Risk free interest rate should be 1, but was {self.model.get_interest_rate(self.time)}")
