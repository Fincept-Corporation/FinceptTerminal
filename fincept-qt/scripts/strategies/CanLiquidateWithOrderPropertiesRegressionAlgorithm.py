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
### Regression algorithm to test we can liquidate our portfolio holdings using order properties
### </summary>
class CanLiquidateWithOrderPropertiesRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2014, 6, 5)
        self.set_end_date(2014, 6, 6)
        self.set_cash(100000)
        
        self.open_exchange = datetime(2014, 6, 6, 10, 0, 0)
        self.close_exchange = datetime(2014, 6, 6, 16, 0, 0)
        self.add_equity("AAPL", resolution = Resolution.MINUTE)
    
    def on_data(self, slice):
        if self.time > self.open_exchange and self.time < self.close_exchange:
            if not self.portfolio.invested:
                self.market_order("AAPL", 10)
            else:
                order_properties = OrderProperties()
                order_properties.time_in_force = TimeInForce.DAY
                tickets = self.liquidate(asynchronous = True, order_properties = order_properties)
                for ticket in tickets:
                    if ticket.SubmitRequest.OrderProperties.TimeInForce != TimeInForce.DAY:
                        raise Exception(f"The TimeInForce for all orders should be daily, but it was {ticket.SubmitRequest.OrderProperties.TimeInForce}")
