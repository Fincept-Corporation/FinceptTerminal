# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-27A6F4AF
# Category: General Strategy
# Description: This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies. In th...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

import itertools

from OptionStrategyFactoryMethodsBaseAlgorithm import *

### <summary>
### This algorithm demonstrate how to use OptionStrategies helper class to batch send orders for common strategies.
### In this case, the algorithm tests the Put Calendar Spread and Short Put Calendar Spread strategies.
### </summary>
class LongAndShortPutCalendarSpreadStrategiesAlgorithm(OptionStrategyFactoryMethodsBaseAlgorithm):

    def expected_orders_count(self) -> int:
        return 4

    def trade_strategy(self, chain: OptionChain, option_symbol: Symbol):
        put_contracts = sorted((contract for contract in chain if contract.right == OptionRight.PUT),
                              key=lambda x: abs(x.strike - chain.underlying.value))
        for strike, group in itertools.groupby(put_contracts, lambda x: x.strike):
            contracts = sorted(group, key=lambda x: x.expiry)
            if len(contracts) < 2: continue

            self._near_expiration = contracts[0].expiry
            self._far_expiration = contracts[1].expiry

            self._put_calendar_spread = OptionStrategies.put_calendar_spread(option_symbol, strike, self._near_expiration, self._far_expiration)
            self._short_put_calendar_spread = OptionStrategies.short_put_calendar_spread(option_symbol, strike, self._near_expiration, self._far_expiration)
            self.buy(self._put_calendar_spread, 2)
            return

    def assert_strategy_position_group(self, position_group: IPositionGroup, option_symbol: Symbol):
        positions = list(position_group.positions)
        if len(positions) != 2:
            raise Exception(f"Expected position group to have 2 positions. Actual: {len(positions)}")

        near_expiration_position = next((position for position in positions
                                       if position.symbol.id.option_right == OptionRight.PUT and position.symbol.id.date == self._near_expiration),
                                      None)
        if near_expiration_position is None or near_expiration_position.quantity != -2:
            raise Exception(f"Expected near expiration position to be -2. Actual: {near_expiration_position.quantity}")

        far_expiration_position = next((position for position in positions
                                      if position.symbol.id.option_right == OptionRight.PUT and position.symbol.id.date == self._far_expiration),
                                     None)
        if far_expiration_position is None or far_expiration_position.quantity != 2:
            raise Exception(f"Expected far expiration position to be 2. Actual: {far_expiration_position.quantity}")

    def liquidate_strategy(self):
        # We should be able to close the position using the inverse strategy (a short put calendar spread)
        self.buy(self._short_put_calendar_spread, 2)
