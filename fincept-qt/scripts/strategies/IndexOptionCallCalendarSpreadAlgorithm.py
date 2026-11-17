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

class IndexOptionCallCalendarSpreadAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 1, 1)
        self.set_end_date(2021, 1, 1)
        self.set_cash(50000)

        self.vxz = self.add_equity("VXZ", Resolution.MINUTE).symbol
        self.spy = self.add_equity("SPY", Resolution.MINUTE).symbol

        index = self.add_index("VIX", Resolution.MINUTE).symbol
        option = self.add_index_option(index, "VIXW", Resolution.MINUTE)
        option.set_filter(lambda x: x.strikes(-2, 2).expiration(15, 45))
        
        self.vixw = option.symbol
        self.multiplier = option.symbol_properties.contract_multiplier
        self.legs = []
        self.expiry = datetime.max

    def on_data(self, slice: Slice) -> None:
        # Liquidate if the shorter term option is about to expire
        if self.expiry < self.time + timedelta(2) and all([slice.contains_key(x.symbol) for x in self.legs]):
            self.liquidate()
        # Return if there is any opening position
        elif [leg for leg in self.legs if self.portfolio[leg.symbol].invested]:
            return

        # Get the OptionChain
        chain = slice.option_chains.get(self.vixw)
        if not chain: return

        # Get ATM strike price
        strike = sorted(chain, key = lambda x: abs(x.strike - chain.underlying.value))[0].strike
        
        # Select the ATM call Option contracts and sort by expiration date
        calls = sorted([i for i in chain if i.strike == strike and i.right == OptionRight.CALL], 
                        key=lambda x: x.expiry)
        if len(calls) < 2: return
        self.expiry = calls[0].expiry

        # Create combo order legs
        self.legs = [
            Leg.create(calls[0].symbol, -1),
            Leg.create(calls[-1].symbol, 1),
            Leg.create(self.vxz, -100),
            Leg.create(self.spy, -10)
        ]
        quantity = self.portfolio.total_portfolio_value // \
            sum([abs(self.securities[x.symbol].price * x.quantity * 
                 (self.multiplier if x.symbol.id.security_type == SecurityType.INDEX_OPTION else 1))
                 for x in self.legs])
        self.combo_market_order(self.legs, -quantity, asynchronous=True)