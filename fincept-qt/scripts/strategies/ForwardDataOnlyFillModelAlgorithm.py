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
### Example of custom fill model for security to only fill bars of data obtained after the order was placed. This is to encourage more
### pessimistic fill models and eliminate the possibility to fill on old market data that may not be relevant.
### </summary>
class ForwardDataOnlyFillModelAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013,10,1)
        self.set_end_date(2013,10,31)

        self.security = self.add_equity("SPY", Resolution.HOUR)
        self.security.set_fill_model(ForwardDataOnlyFillModel())

        self.schedule.on(self.date_rules.week_start(), self.time_rules.after_market_open(self.security.symbol), self.trade)

    def trade(self):
        if not self.portfolio.invested:
            if self.time.hour != 9 or self.time.minute != 30:
                raise Exception(f"Unexpected event time {self.time}")

            ticket = self.buy("SPY", 1)
            if ticket.status != OrderStatus.SUBMITTED:
                raise Exception(f"Unexpected order status {ticket.status}")

    def on_order_event(self, order_event: OrderEvent):
        self.debug(f"OnOrderEvent:: {order_event}")
        if order_event.status == OrderStatus.FILLED and (self.time.hour != 10 or self.time.minute != 0):
            raise Exception(f"Unexpected fill time {self.time}")

class ForwardDataOnlyFillModel(EquityFillModel):
    def fill(self, parameters: FillModelParameters):
        order_local_time = Extensions.convert_from_utc(parameters.order.time, parameters.security.exchange.time_zone)
        for data_type in [ QuoteBar, TradeBar, Tick ]:
            data = parameters.security.cache.get_data[data_type]()
            if not data is None and order_local_time <= data.end_time:
                return super().fill(parameters)
        return Fill([])
