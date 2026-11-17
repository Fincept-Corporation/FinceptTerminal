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

#region imports
from AlgorithmImports import *
#endregion

class IndexOptionIronCondorAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2019, 9, 1)
        self.set_end_date(2019, 11, 1)
        self.set_cash(100000)

        index = self.add_index("SPX", Resolution.MINUTE).symbol
        option = self.add_index_option(index, "SPXW", Resolution.MINUTE)
        option.set_filter(lambda x: x.weeklys_only().strikes(-5, 5).expiration(0, 14))
        self.spxw = option.symbol

        self.bb = self.bb(index, 10, 2, resolution=Resolution.DAILY)
        self.warm_up_indicator(index, self.bb)
        
    def on_data(self, slice: Slice) -> None:
        if self.portfolio.invested: return

        # Get the OptionChain
        chain = slice.option_chains.get(self.spxw)
        if not chain: return

        # Get the closest expiry date
        expiry = min([x.expiry for x in chain])
        chain = [x for x in chain if x.expiry == expiry]

        # Separate the call and put contracts and sort by Strike to find OTM contracts
        calls = sorted([x for x in chain if x.right == OptionRight.CALL], key=lambda x: x.strike, reverse=True)
        puts = sorted([x for x in chain if x.right == OptionRight.PUT], key=lambda x: x.strike)
        if len(calls) < 3 or len(puts) < 3: return

        # Create combo order legs
        price = self.bb.price.current.value
        quantity = 1
        if price > self.bb.upper_band.current.value or price < self.bb.lower_band.current.value:
            quantity = -1

        legs = [
            Leg.create(calls[0].symbol, quantity),
            Leg.create(puts[0].symbol, quantity),
            Leg.create(calls[2].symbol, -quantity),
            Leg.create(puts[2].symbol, -quantity)
        ]

        self.combo_market_order(legs, 10, asynchronous=True)