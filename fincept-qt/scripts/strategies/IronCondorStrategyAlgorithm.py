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
### In this case, the algorithm tests the Iron Condor strategy.
### </summary>
class IronCondorStrategyAlgorithm(OptionStrategyFactoryMethodsBaseAlgorithm):

    def expected_orders_count(self) -> int:
        return 8

    def trade_strategy(self, chain: OptionChain, option_symbol: Symbol):
        for expiry, group in itertools.groupby(chain, lambda x: x.expiry):
            contracts = sorted(group, key=lambda x: x.strike)
            if len(contracts) < 4:continue

            put_contracts = [x for x in contracts if x.right == OptionRight.PUT]
            if len(put_contracts) < 2: continue
            long_put_strike = put_contracts[0].strike
            short_put_strike = put_contracts[1].strike

            call_contracts = [x for x in contracts if x.right == OptionRight.CALL and x.strike > short_put_strike]
            if len(call_contracts) < 2: continue
            short_call_strike = call_contracts[0].strike
            long_call_strike = call_contracts[1].strike

            self._iron_condor = OptionStrategies.iron_condor(option_symbol, long_put_strike, short_put_strike, short_call_strike, long_call_strike, expiry)
            self.buy(self._iron_condor, 2)
            return

    def assert_strategy_position_group(self, position_group: IPositionGroup, option_symbol: Symbol):
        positions = list(position_group.positions)
        if len(positions) != 4:
            raise Exception(f"Expected position group to have 4 positions. Actual: {len(positions)}")

        ordered_strikes = sorted((leg.strike for leg in self._iron_condor.option_legs))

        long_put_strike = ordered_strikes[0]
        long_put_position = next((x for x in position_group.positions
                                if x.symbol.id.option_right == OptionRight.PUT and x.symbol.id.strike_price == long_put_strike),
                               None)
        if long_put_position is None or long_put_position.quantity != 2:
            raise Exception(f"Expected long put position quantity to be 2. Actual: {long_put_position.quantity}")

        short_put_strike = ordered_strikes[1]
        short_put_position = next((x for x in position_group.positions
                                 if x.symbol.id.option_right == OptionRight.PUT and x.symbol.id.strike_price == short_put_strike),
                                None)
        if short_put_position is None or short_put_position.quantity != -2:
            raise Exception(f"Expected short put position quantity to be -2. Actual: {short_put_position.quantity}")

        short_call_strike = ordered_strikes[2]
        short_call_position = next((x for x in position_group.positions
                                  if x.symbol.id.option_right == OptionRight.CALL and x.symbol.id.strike_price == short_call_strike),
                                 None)
        if short_call_position is None or short_call_position.quantity != -2:
            raise Exception(f"Expected short call position quantity to be -2. Actual: {short_call_position.quantity}")

        long_call_strike = ordered_strikes[3]
        long_call_position = next((x for x in position_group.positions
                                 if x.symbol.id.option_right == OptionRight.CALL and x.symbol.id.strike_price == long_call_strike),
                                None)
        if long_call_position is None or long_call_position.quantity != 2:
            raise Exception(f"Expected long call position quantity to be 2. Actual: {long_call_position.quantity}")

    def liquidate_strategy(self):
        # We should be able to close the position by selling the strategy
        self.sell(self._iron_condor, 2)
