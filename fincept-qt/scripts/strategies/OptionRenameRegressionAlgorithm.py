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
### This is an option split regression algorithm
### </summary>
### <meta name="tag" content="options" />
### <meta name="tag" content="regression test" />
class OptionRenameRegressionAlgorithm(QCAlgorithm):

    def initialize(self):

        self.set_cash(1000000)
        self.set_start_date(2013,6,28)
        self.set_end_date(2013,7,2)
        option = self.add_option("TFCFA")

        # set our strike/expiry filter for this option chain
        option.set_filter(-1, 1, timedelta(0), timedelta(3650))
        # use the underlying equity as the benchmark
        self.set_benchmark("TFCFA")

    def on_data(self, slice):
        ''' Event - v3.0 DATA EVENT HANDLER: (Pattern) Basic template for user to override for receiving all subscription data in a single event
        <param name="slice">The current slice of data keyed by symbol string</param> '''
        if not self.portfolio.invested: 
            for kvp in slice.option_chains:
                chain = kvp.value
                if self.time.day == 28 and self.time.hour > 9 and self.time.minute > 0:
    
                    contracts = [i for i in sorted(chain, key=lambda x:x.expiry) 
                                         if i.right ==  OptionRight.CALL and 
                                            i.strike == 33 and
                                            i.expiry.date() == datetime(2013,8,17).date()]
                    if contracts:
                        # Buying option
                        contract = contracts[0]
                        self.buy(contract.symbol, 1)
                        # Buy the undelying stock
                        underlying_symbol = contract.symbol.underlying
                        self.buy (underlying_symbol, 100)
                        # check
                        if float(contract.ask_price) != 1.1:
                            raise ValueError("Regression test failed: current ask price was not loaded from NWSA backtest file and is not $1.1")
        elif self.time.day == 2 and self.time.hour > 14 and self.time.minute > 0:
            for kvp in slice.option_chains:
                chain = kvp.value
                self.liquidate()
                contracts = [i for i in sorted(chain, key=lambda x:x.expiry) 
                                        if i.right ==  OptionRight.CALL and 
                                           i.strike == 33 and
                                           i.expiry.date() == datetime(2013,8,17).date()]
            if contracts:
                contract = contracts[0]
                self.log("Bid Price" + str(contract.bid_price))
                if float(contract.bid_price) != 0.05:
                    raise ValueError("Regression test failed: current bid price was not loaded from FOXA file and is not $0.05")

    def on_order_event(self, order_event):
        self.log(str(order_event))
