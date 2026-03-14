# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-82AD115E
# Category: General Strategy
# Description: This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies. In th...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from OptionStrategyFactoryMethodsBaseAlgorithm import *

### <summary>
### This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies.
### In this case, the algorithm tests the Naked Put strategy.
### </summary>
class NakedPutStrategyAlgorithm(OptionStrategyFactoryMethodsBaseAlgorithm):

    def expected_orders_count(self) -> int:
        return 2

    def trade_strategy(self, chain: OptionChain, option_symbol: Symbol):
        contracts = sorted(sorted(chain, key = lambda x: abs(chain.underlying.price - x.strike)),
                           key = lambda x: x.expiry, reverse=True)

        if len(contracts) == 0: return
        contract = contracts[0]
        if contract != None:
            self._naked_put = OptionStrategies.naked_put(option_symbol, contract.strike, contract.expiry)
            self.buy(self._naked_put, 2)

    def assert_strategy_position_group(self, position_group: IPositionGroup, option_symbol: Symbol):
        positions = list(position_group.positions)
        if len(positions) != 1:
            raise Exception(f"Expected position group to have 1 positions. Actual: {len(positions)}")

        option_position = [position for position in positions if position.symbol.security_type == SecurityType.OPTION][0]
        if option_position.symbol.id.option_right != OptionRight.PUT:
            raise Exception(f"Expected option position to be a put. Actual: {option_position.symbol.id.option_right}")

        expected_option_position_quantity = -2

        if option_position.quantity != expected_option_position_quantity:
            raise Exception(f"Expected option position quantity to be {expected_option_position_quantity}. Actual: {option_position.quantity}")

    def liquidate_strategy(self):
        # Now we can liquidate by selling the strategy
        self.sell(self._naked_put, 2)
