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

class IndexOptionBullPutSpreadAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2019, 1, 1)
        self.set_end_date(2020, 1, 1)
        self.set_cash(100000)

        index = self.add_index("SPX", Resolution.MINUTE).symbol
        option = self.add_index_option(index, "SPXW", Resolution.MINUTE)
        option.set_filter(lambda x: x.weeklys_only().strikes(-10, -5).expiration(0, 0))
        
        self.spxw = option.symbol
        self.tickets = []

    def on_data(self, slice: Slice) -> None:
        # Return if open position exists
        if any([self.portfolio[x.symbol].invested for x in self.tickets]):
            return

        # Get option chain
        chain = slice.option_chains.get(self.spxw)
        if not chain: return

        # Get the nearest expiry date of the contracts
        expiry = min([x.expiry for x in chain])
        
        # Select the put Option contracts with the nearest expiry and sort by strike price
        puts = sorted([i for i in chain if i.expiry == expiry and i.right == OptionRight.PUT], 
                        key=lambda x: x.strike)
        if len(puts) < 2: return

        # Buy the bull put spread
        bull_call_spread = OptionStrategies.bull_put_spread(self.spxw, puts[-1].strike, puts[0].strike, expiry)
        self.tickets = self.buy(bull_call_spread, 1)