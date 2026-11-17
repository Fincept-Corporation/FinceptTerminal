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

from OptionStrategyFactoryMethodsBaseAlgorithm import *

### <summary>
### This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies.
### In this case, the algorithm tests the Naked Call strategy.
### </summary>
class NakedCallStrategyAlgorithm(OptionStrategyFactoryMethodsBaseAlgorithm):

    def expected_orders_count(self) -> int:
        return 2

    def trade_strategy(self, chain: OptionChain, option_symbol: Symbol):
        contracts = sorted(sorted(chain, key = lambda x: abs(chain.underlying.price - x.strike)),
                           key = lambda x: x.expiry, reverse=True)

        if len(contracts) == 0: return
        contract = contracts[0]
        if contract != None:
            self._naked_call = OptionStrategies.naked_call(option_symbol, contract.strike, contract.expiry)
            self.buy(self._naked_call, 2)

    def assert_strategy_position_group(self, position_group: IPositionGroup, option_symbol: Symbol):
        positions = list(position_group.positions)
        if len(positions) != 1:
            raise Exception(f"Expected position group to have 1 positions. Actual: {len(positions)}")

        option_position = [position for position in positions if position.symbol.security_type == SecurityType.OPTION][0]
        if option_position.symbol.id.option_right != OptionRight.CALL:
            raise Exception(f"Expected option position to be a call. Actual: {option_position.symbol.id.option_right}")

        expected_option_position_quantity = -2

        if option_position.quantity != expected_option_position_quantity:
            raise Exception(f"Expected option position quantity to be {expected_option_position_quantity}. Actual: {option_position.quantity}")

    def liquidate_strategy(self):
        # We can liquidate by selling the strategy
        self.sell(self._naked_call, 2)
