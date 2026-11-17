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
### Regression algorithm asserting we can specify a custom security data filter
### </summary>
class CustomSecurityDataFilterRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_cash(2500000)
        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,7)

        security = self.add_security(SecurityType.EQUITY, "SPY")
        security.set_data_filter(CustomDataFilter())
        self.data_points = 0

    def on_data(self, data):
        self.data_points += 1
        self.set_holdings("SPY", 0.2)
        if self.data_points > 5:
            raise Exception("There should not be more than 5 data points, but there were " + str(self.data_points))


class CustomDataFilter(SecurityDataFilter):
    def filter(self, vehicle: Security, data: BaseData) -> bool:
        """
        Skip data after 9:35am
        """
        if data.time >= datetime(2013,10,7,9,35,0):
            return False
        else:
            return True
