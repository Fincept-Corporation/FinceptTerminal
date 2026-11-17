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
from System.Collections.Generic import List
from QuantConnect.Data.Custom.IconicTypes import *

### <summary>
### Provides an example algorithm showcasing the Security.data features
### </summary>
class DynamicSecurityDataRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 10, 22)
        self.set_end_date(2015, 10, 30)

        self.ticker = "GOOGL"
        self.equity = self.add_equity(self.ticker, Resolution.DAILY)

        custom_linked_equity = self.add_data(LinkedData, self.ticker, Resolution.DAILY)
        
        first_linked_data = LinkedData()
        first_linked_data.count = 100
        first_linked_data.symbol = custom_linked_equity.symbol
        first_linked_data.end_time = self.start_date
        
        second_linked_data = LinkedData()
        second_linked_data.count = 100
        second_linked_data.symbol = custom_linked_equity.symbol
        second_linked_data.end_time = self.start_date
        
        # Adding linked data manually to cache for example purposes, since
        # LinkedData is a type used for testing and doesn't point to any real data.
        custom_linked_equity_type = list(custom_linked_equity.subscriptions)[0].type
        custom_linked_data = List[LinkedData]()
        custom_linked_data.add(first_linked_data)
        custom_linked_data.add(second_linked_data)
        self.equity.cache.add_data_list(custom_linked_data, custom_linked_equity_type, False) 

    def on_data(self, data):
        # The Security object's Data property provides convenient access
        # to the various types of data related to that security. You can
        # access not only the security's price data, but also any custom
        # data that is mapped to the security, such as our SEC reports.

        # 1. Get the most recent data point of a particular type:
        # 1.a Using the generic method, Get(T): => T
        custom_linked_data = self.equity.data.get(LinkedData)
        self.log("{}: LinkedData: {}".format(self.time, str(custom_linked_data)))

        # 2. Get the list of data points of a particular type for the most recent time step:
        # 2.a Using the generic method, GetAll(T): => IReadOnlyList<T>
        custom_linked_data_list = self.equity.data.get_all(LinkedData)
        self.log("{}: LinkedData: {}".format(self.time, len(custom_linked_data_list)))

        if not self.portfolio.invested:
            self.buy(self.equity.symbol, 10)
