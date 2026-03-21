# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-FD19D095
# Category: General Strategy
# Description: This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies. In th...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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
