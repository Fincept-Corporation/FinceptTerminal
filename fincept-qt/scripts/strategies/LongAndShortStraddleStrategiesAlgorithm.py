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

import itertools
from AlgorithmImports import *

from OptionStrategyFactoryMethodsBaseAlgorithm import *

### <summary>
### This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies.
### In this case, the algorithm tests the Straddle and Short Straddle strategies.
### </summary>
class LongAndShortStraddleStrategiesAlgorithm(OptionStrategyFactoryMethodsBaseAlgorithm):

    def expected_orders_count(self) -> int:
        return 4

    def trade_strategy(self, chain: OptionChain, option_symbol: Symbol):
        contracts = sorted(sorted(chain, key=lambda x: abs(chain.underlying.price - x.strike)),
                           key=lambda x: x.expiry, reverse=True)
        grouped_contracts = [list(group) for _, group in itertools.groupby(contracts, lambda x: (x.strike, x.expiry))]
        grouped_contracts = (group
                            for group in grouped_contracts
                            if (any(contract.right == OptionRight.CALL for contract in group) and
                                any(contract.right == OptionRight.PUT for contract in group)))
        contracts = next(grouped_contracts, [])

        if len(contracts) == 0:
            return

        contract = contracts[0]
        if contract is not None:
            self._straddle = OptionStrategies.straddle(option_symbol, contract.strike, contract.expiry)
            self._short_straddle = OptionStrategies.short_straddle(option_symbol, contract.strike, contract.expiry)
            self.buy(self._straddle, 2)

    def assert_strategy_position_group(self, position_group: IPositionGroup, option_symbol: Symbol):
        positions = list(position_group.positions)
        if len(positions) != 2:
            raise Exception(f"Expected position group to have 2 positions. Actual: {len(positions)}")

        call_position = next((position for position in positions if position.symbol.id.option_right == OptionRight.CALL), None)
        if call_position is None:
            raise Exception("Expected position group to have a call position")

        put_position = next((position for position in positions if position.symbol.id.option_right == OptionRight.PUT), None)
        if put_position is None:
            raise Exception("Expected position group to have a put position")

        expected_call_position_quantity = 2
        expected_put_position_quantity = 2

        if call_position.quantity != expected_call_position_quantity:
            raise Exception(f"Expected call position quantity to be {expected_call_position_quantity}. Actual: {call_position.quantity}")

        if put_position.quantity != expected_put_position_quantity:
            raise Exception(f"Expected put position quantity to be {expected_put_position_quantity}. Actual: {put_position.quantity}")

    def liquidate_strategy(self):
        # We should be able to close the position using the inverse strategy (a short straddle)
        self.buy(self._short_straddle, 2)
