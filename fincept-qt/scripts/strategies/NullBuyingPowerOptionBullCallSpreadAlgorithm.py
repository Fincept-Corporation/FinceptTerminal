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
### Shows how setting to use the SecurityMarginModel.null (or BuyingPowerModel.NULL)
### to disable the sufficient margin call verification.
### See also: <see cref="OptionEquityBullCallSpreadRegressionAlgorithm"/>
### </summary>
### <meta name="tag" content="reality model" />
class NullBuyingPowerOptionBullCallSpreadAlgorithm(QCAlgorithm):
    def initialize(self):

        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(200000)

        self.set_security_initializer(lambda security: security.set_margin_model(SecurityMarginModel.NULL))
        self.portfolio.set_positions(SecurityPositionGroupModel.NULL);

        equity = self.add_equity("GOOG")
        option = self.add_option(equity.symbol)
        self.option_symbol = option.symbol

        option.set_filter(-2, 2, 0, 180)
        
    def on_data(self, slice):
        if self.portfolio.invested or not self.is_market_open(self.option_symbol):
            return
       
        chain = slice.option_chains.get(self.option_symbol)
        if chain:
            call_contracts = [x for x in chain if x.right == OptionRight.CALL]

            expiry = min(x.expiry for x in call_contracts)

            call_contracts = sorted([x for x in call_contracts if x.expiry == expiry],
                key = lambda x: x.strike)

            long_call = call_contracts[0]
            short_call = [x for x in call_contracts if x.strike > long_call.strike][0]

            quantity = 1000

            tickets = [
                self.market_order(short_call.symbol, -quantity),
                self.market_order(long_call.symbol, quantity)
            ]
                
            for ticket in tickets:
                if ticket.status != OrderStatus.FILLED:
                    raise Exception(f"There should be no restriction on buying {ticket.quantity} of {ticket.symbol} with BuyingPowerModel.NULL")


    def on_end_of_algorithm(self) -> None:
        if self.portfolio.total_margin_used != 0:
            raise Exception("The TotalMarginUsed should be zero to avoid margin calls.")
