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
### Regression algorithm asserting the behavior of specifying a null position group allowing us to fill orders which would be invalid if not
### </summary>
class NullMarginMultipleOrdersRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(10000)

        # override security position group model
        self.portfolio.set_positions(SecurityPositionGroupModel.NULL)
        # override margin requirements
        self.set_security_initializer(lambda security: security.set_buying_power_model(ConstantBuyingPowerModel(1)))

        equity = self.add_equity("GOOG", leverage=4, fill_forward=True)
        option = self.add_option(equity.symbol, fill_forward=True)
        self._option_symbol = option.symbol

        option.set_filter(lambda u: u.strikes(-2, +2).expiration(0, 180))

    def on_data(self, data: Slice):
        if not self.portfolio.invested:
            if self.is_market_open(self._option_symbol):
                chain = data.option_chains.get_value(self._option_symbol)
                if chain is not None:
                    call_contracts = [contract for contract in chain if contract.right == OptionRight.CALL]
                    call_contracts.sort(key=lambda x: (x.expiry, 1/ x.strike), reverse=True)

                    option_contract = call_contracts[0]
                    self.market_order(option_contract.symbol.underlying, 1000)
                    self.market_order(option_contract.symbol, -10)

                    if self.portfolio.total_margin_used != 1010:
                        raise ValueError(f"Unexpected margin used {self.portfolio.total_margin_used}")
