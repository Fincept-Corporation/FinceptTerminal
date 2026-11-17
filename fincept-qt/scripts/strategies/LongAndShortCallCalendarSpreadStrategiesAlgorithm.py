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

import itertools

from OptionStrategyFactoryMethodsBaseAlgorithm import *

### <summary>
### This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies.
### In this case, the algorithm tests the Call Calendar Spread and Short Call Calendar Spread strategies.
### </summary>
class LongAndShortCallCalendarSpreadStrategiesAlgorithm(OptionStrategyFactoryMethodsBaseAlgorithm):

    def expected_orders_count(self) -> int:
        return 4

    def trade_strategy(self, chain: OptionChain, option_symbol: Symbol):
        call_contracts = sorted((contract for contract in chain if contract.right == OptionRight.CALL),
                           key=lambda x: abs(x.strike - chain.underlying.value))
        for strike, group in itertools.groupby(call_contracts, lambda x: x.strike):
            contracts = sorted(group, key=lambda x: x.expiry)
            if len(contracts) < 2: continue

            self._near_expiration = contracts[0].expiry
            self._far_expiration = contracts[1].expiry

            self._call_calendar_spread = OptionStrategies.call_calendar_spread(option_symbol, strike, self._near_expiration, self._far_expiration)
            self._short_call_calendar_spread = OptionStrategies.short_call_calendar_spread(option_symbol, strike, self._near_expiration, self._far_expiration)
            self.buy(self._call_calendar_spread, 2)
            return

    def assert_strategy_position_group(self, position_group: IPositionGroup, option_symbol: Symbol):
        positions = list(position_group.positions)
        if len(positions) != 2:
            raise Exception(f"Expected position group to have 2 positions. Actual: {len(positions)}")

        near_expiration_position = next((position for position in positions
                                       if position.symbol.id.option_right == OptionRight.CALL and position.symbol.id.date == self._near_expiration),
                                      None)
        if near_expiration_position is None or near_expiration_position.quantity != -2:
            raise Exception(f"Expected near expiration position to be -2. Actual: {near_expiration_position.quantity}")

        far_expiration_position = next((position for position in positions
                                      if position.symbol.id.option_right == OptionRight.CALL and position.symbol.id.date == self._far_expiration),
                                     None)
        if far_expiration_position is None or far_expiration_position.quantity != 2:
            raise Exception(f"Expected far expiration position to be 2. Actual: {far_expiration_position.quantity}")

    def liquidate_strategy(self):
        # We should be able to close the position using the inverse strategy (a short call calendar spread)
        self.buy(self._short_call_calendar_spread, 2)
