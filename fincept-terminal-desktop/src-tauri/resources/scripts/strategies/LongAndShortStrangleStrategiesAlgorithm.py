# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-73CA6D31
# Category: General Strategy
# Description: This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies. In th...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
import itertools
from AlgorithmImports import *

from OptionStrategyFactoryMethodsBaseAlgorithm import *

### <summary>
### This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies.
### In this case, the algorithm tests the Strangle and Short Strangle strategies.
### </summary>
class LongAndShortStrangleStrategiesAlgorithm(OptionStrategyFactoryMethodsBaseAlgorithm):

    def expected_orders_count(self) -> int:
        return 4

    def trade_strategy(self, chain: OptionChain, option_symbol: Symbol):
        contracts = sorted(sorted(chain, key=lambda x: abs(chain.underlying.price - x.strike)),
                           key=lambda x: x.expiry, reverse=True)
        grouped_contracts = (list(group) for _, group in itertools.groupby(contracts, lambda x: x.expiry))

        call_contract = None
        put_contract = None
        for group in grouped_contracts:
            call_contracts = sorted((contract for contract in group if contract.right == OptionRight.CALL),
                                   key=lambda x: x.strike, reverse=True)
            put_contracts = sorted((contract for contract in group if contract.right == OptionRight.PUT),
                                  key=lambda x: x.strike)

            if len(call_contracts) > 0 and len(put_contracts) > 0 and call_contracts[0].strike > put_contracts[0].strike:
                call_contract = call_contracts[0]
                put_contract = put_contracts[0]
                break

        if call_contract is not None and put_contract is not None:
            self._strangle = OptionStrategies.strangle(option_symbol, call_contract.strike, put_contract.strike, call_contract.expiry)
            self._short_strangle = OptionStrategies.short_strangle(option_symbol, call_contract.strike, put_contract.strike,
                                                                  call_contract.expiry)
            self.buy(self._strangle, 2)

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
        # We should be able to close the position using the inverse strategy (a short strangle)
        self.buy(self._short_strangle, 2)
