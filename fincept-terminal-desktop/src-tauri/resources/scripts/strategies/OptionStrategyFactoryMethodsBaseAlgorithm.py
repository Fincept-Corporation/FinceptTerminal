# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-CB64E8E9
# Category: Options
# Description: This base algorithm demonstrates how to use OptionStrategies helper class to batch send orders for common strategies
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from QuantConnect.Securities.Positions import IPositionGroup

### <summary>
### This base algorithm demonstrates how to use OptionStrategies helper class to batch send orders for common strategies.
### </summary>
class OptionStrategyFactoryMethodsBaseAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(1000000)

        option = self.add_option("GOOG")
        self._option_symbol = option.symbol

        option.set_filter(-2, +2, 0, 180)

        self.set_benchmark("GOOG")

    def on_data(self, slice):
        if not self.portfolio.invested:
            chain = slice.option_chains.get(self._option_symbol)
            if chain is not None:
                self.trade_strategy(chain, self._option_symbol)
        else:
            # Verify that the strategy was traded
            position_group = list(self.portfolio.positions.groups)[0]

            buying_power_model = position_group.buying_power_model
            if not isinstance(buying_power_model, OptionStrategyPositionGroupBuyingPowerModel):
                raise Exception("Expected position group buying power model type: OptionStrategyPositionGroupBuyingPowerModel. "
                                f"Actual: {type(position_group.buying_power_model).__name__}")

            self.assert_strategy_position_group(position_group, self._option_symbol)

            # Now we should be able to close the position
            self.liquidate_strategy()

            # We can quit now, no more testing required
            self.quit()

    def on_end_of_algorithm(self):
        if self.portfolio.invested:
            raise Exception("Expected no holdings at end of algorithm")

        orders_count = len(list(self.transactions.get_orders(lambda order: order.status == OrderStatus.FILLED)))
        if orders_count != self.expected_orders_count():
            raise Exception(f"Expected {self.expected_orders_count()} orders to have been submitted and filled, "
                            f"half for buying the strategy and the other half for the liquidation. Actual {orders_count}")

    def expected_orders_count(self) -> int:
        raise NotImplementedError("ExpectedOrdersCount method is not implemented")

    def trade_strategy(self, chain: OptionChain, option_symbol: Symbol) -> None:
        raise NotImplementedError("TradeStrategy method is not implemented")

    def assert_strategy_position_group(self, position_group: IPositionGroup, option_symbol: Symbol) -> None:
        raise NotImplementedError("AssertStrategyPositionGroup method is not implemented")

    def liquidate_strategy(self) -> None:
        raise NotImplementedError("LiquidateStrategy method is not implemented")
