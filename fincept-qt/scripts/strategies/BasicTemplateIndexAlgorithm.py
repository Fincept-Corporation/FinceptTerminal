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
# limitations under the License
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

from AlgorithmImports import *

class BasicTemplateIndexAlgorithm(QCAlgorithm):
    def initialize(self) -> None:
        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 1, 18)
        self.set_cash(1000000)

        # Use indicator for signal; but it cannot be traded
        self.spx = self.add_index("SPX", Resolution.MINUTE).symbol

        # Trade on SPX ITM calls
        self.spx_option = Symbol.create_option(
            self.spx,
            Market.USA,
            OptionStyle.EUROPEAN,
            OptionRight.CALL,
            3200,
            datetime(2021, 1, 15)
        )

        self.add_index_option_contract(self.spx_option, Resolution.MINUTE)

        self.ema_slow = self.ema(self.spx, 80)
        self.ema_fast = self.ema(self.spx, 200)

    def on_data(self, data: Slice):
        if self.spx not in data.bars or self.spx_option not in data.bars:
            return

        if not self.ema_slow.is_ready:
            return

        if self.ema_fast > self.ema_slow:
            self.set_holdings(self.spx_option, 1)
        else:
            self.liquidate()

    def on_end_of_algorithm(self) -> None:
        if self.portfolio[self.spx].total_sale_volume > 0:
            raise Exception("Index is not tradable.")
