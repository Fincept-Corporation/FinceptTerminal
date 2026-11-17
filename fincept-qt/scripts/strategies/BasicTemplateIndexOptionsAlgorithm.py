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

class BasicTemplateIndexOptionsAlgorithm(QCAlgorithm):
    def initialize(self) -> None:
        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 2, 1)
        self.set_cash(1000000)

        self.spx = self.add_index("SPX", Resolution.MINUTE).symbol
        spx_options = self.add_index_option(self.spx, Resolution.MINUTE)
        spx_options.set_filter(lambda x: x.calls_only())

        self.ema_slow = self.ema(self.spx, 80)
        self.ema_fast = self.ema(self.spx, 200)

    def on_data(self, data: Slice) -> None:
        if self.spx not in data.bars or not self.ema_slow.is_ready:
            return

        for chain in data.option_chains.values():
            for contract in chain.contracts.values():
                if self.portfolio.invested:
                    continue

                if (self.ema_fast > self.ema_slow and contract.right == OptionRight.CALL) or \
                    (self.ema_fast < self.ema_slow and contract.right == OptionRight.PUT):

                    self.liquidate(self.invert_option(contract.symbol))
                    self.market_order(contract.symbol, 1)

    def on_end_of_algorithm(self) -> None:
        if self.portfolio[self.spx].total_sale_volume > 0:
            raise Exception("Index is not tradable.")

        if self.portfolio.total_sale_volume == 0:
            raise Exception("Trade volume should be greater than zero by the end of this algorithm")

    def invert_option(self, symbol: Symbol) -> Symbol:
        return Symbol.create_option(
            symbol.underlying,
            symbol.id.market,
            symbol.id.option_style,
            OptionRight.PUT if symbol.id.option_right == OptionRight.CALL else OptionRight.CALL,
            symbol.id.strike_price,
            symbol.id.date
        )
