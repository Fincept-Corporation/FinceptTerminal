# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0AD9A342
# Category: General Strategy
# Description: This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies. In th...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from OptionStrategyFactoryMethodsBaseAlgorithm import *

### <summary>
### This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies.
### In this case, the algorithm tests the Covered and Protective Call strategies.
### </summary>
class CoveredAndProtectiveCallStrategiesAlgorithm(OptionStrategyFactoryMethodsBaseAlgorithm):

    def expected_orders_count(self) -> int:
        return 4

    def trade_strategy(self, chain: OptionChain, option_symbol: Symbol):
        contracts = sorted(sorted(chain, key = lambda x: abs(chain.underlying.price - x.strike)),
                           key = lambda x: x.expiry, reverse=True)

        if len(contracts) == 0: return
        contract = contracts[0]
        if contract != None:
            self._covered_call = OptionStrategies.covered_call(option_symbol, contract.strike, contract.expiry)
            self._protective_call = OptionStrategies.protective_call(option_symbol, contract.strike, contract.expiry)
            self.buy(self._covered_call, 2)

    def assert_strategy_position_group(self, position_group: IPositionGroup, option_symbol: Symbol):
        positions = list(position_group.positions)
        if len(positions) != 2:
            raise Exception(f"Expected position group to have 2 positions. Actual: {len(positions)}")

        option_position = [position for position in positions if position.symbol.security_type == SecurityType.OPTION][0]
        if option_position.symbol.id.option_right != OptionRight.CALL:
            raise Exception(f"Expected option position to be a call. Actual: {option_position.symbol.id.option_right}")

        underlying_position = [position for position in positions if position.symbol.security_type == SecurityType.EQUITY][0]
        expected_option_position_quantity = -2
        expected_underlying_position_quantity = 2 * self.securities[option_symbol].symbol_properties.contract_multiplier

        if option_position.quantity != expected_option_position_quantity:
            raise Exception(f"Expected option position quantity to be {expected_option_position_quantity}. Actual: {option_position.quantity}")

        if underlying_position.quantity != expected_underlying_position_quantity:
            raise Exception(f"Expected underlying position quantity to be {expected_underlying_position_quantity}. Actual: {underlying_position.quantity}")

    def liquidate_strategy(self):
        # We should be able to close the position using the inverse strategy (a protective call)
        self.buy(self._protective_call, 2)
