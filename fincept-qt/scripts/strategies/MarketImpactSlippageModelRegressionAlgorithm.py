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

class MarketImpactSlippageModelRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 13)
        self.set_cash(10000000)

        spy = self.add_equity("SPY", Resolution.DAILY)
        aapl = self.add_equity("AAPL", Resolution.DAILY)

        spy.set_slippage_model(MarketImpactSlippageModel(self))
        aapl.set_slippage_model(MarketImpactSlippageModel(self))

    def on_data(self, data):
        self.set_holdings("SPY", 0.5)
        self.set_holdings("AAPL", -0.5)

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug(f"Price: {self.securities[order_event.symbol].price}, filled price: {order_event.fill_price}, quantity: {order_event.fill_quantity}")
