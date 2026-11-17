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
### The demonstration algorithm shows some of the most common order methods when working with FutureOption assets.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />

class BasicTemplateFutureOptionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2022, 1, 1)
        self.set_end_date(2022, 2, 1)
        self.set_cash(100000)

        gold_futures = self.add_future(Futures.Metals.GOLD, Resolution.MINUTE)
        gold_futures.set_filter(0, 180)
        self._symbol = gold_futures.symbol
        self.add_future_option(self._symbol, lambda universe: universe.strikes(-5, +5)
                                                                    .calls_only()
                                                                    .back_month()
                                                                    .only_apply_filter_at_market_open())

        # Historical Data
        history = self.history(self._symbol, 60, Resolution.DAILY)
        self.log(f"Received {len(history)} bars from {self._symbol} FutureOption historical data call.")

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.
        Arguments:
            slice: Slice object keyed by symbol containing the stock data
        '''
        # Access Data
        for kvp in data.option_chains:
            underlying_future_contract = kvp.key.underlying
            chain = kvp.value

            if not chain: continue

            for contract in chain:
                self.log(f"""Canonical Symbol: {kvp.key}; 
                    Contract: {contract}; 
                    Right: {contract.right}; 
                    Expiry: {contract.expiry}; 
                    Bid price: {contract.bid_price}; 
                    Ask price: {contract.ask_price}; 
                    Implied Volatility: {contract.implied_volatility}""")

            if not self.portfolio.invested:
                atm_strike = sorted(chain, key = lambda x: abs(chain.underlying.price - x.strike))[0].strike
                selected_contract = sorted([contract for contract in chain if contract.strike == atm_strike], \
                           key = lambda x: x.expiry, reverse=True)[0]
                self.market_order(selected_contract.symbol, 1)

    def on_order_event(self, order_event):
        self.debug("{} {}".format(self.time, order_event.to_string()))
