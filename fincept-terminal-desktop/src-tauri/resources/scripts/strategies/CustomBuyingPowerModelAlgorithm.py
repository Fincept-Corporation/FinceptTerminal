# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-DC4DC088
# Category: General Strategy
# Description: Demonstration of using custom buying power model in backtesting. QuantConnect allows you to model all orders as deepl...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of using custom buying power model in backtesting.
### QuantConnect allows you to model all orders as deeply and accurately as you need.
### </summary>
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="transaction fees and slippage" />
### <meta name="tag" content="custom buying power models" />
class CustomBuyingPowerModelAlgorithm(QCAlgorithm):
    '''Demonstration of using custom buying power model in backtesting.
    QuantConnect allows you to model all orders as deeply and accurately as you need.'''

    def initialize(self):
        self.set_start_date(2013,10,1)   # Set Start Date
        self.set_end_date(2013,10,31)    # Set End Date
        security = self.add_equity("SPY", Resolution.HOUR)
        self.spy = security.symbol

        # set the buying power model
        security.set_buying_power_model(CustomBuyingPowerModel())

    def on_data(self, slice):
        if self.portfolio.invested:
            return

        quantity = self.calculate_order_quantity(self.spy, 1)
        if quantity % 100 != 0:
            raise Exception(f'CustomBuyingPowerModel only allow quantity that is multiple of 100 and {quantity} was found')

        # We normally get insufficient buying power model, but the
        # CustomBuyingPowerModel always says that there is sufficient buying power for the orders
        self.market_order(self.spy, quantity * 10)


class CustomBuyingPowerModel(BuyingPowerModel):
    def get_maximum_order_quantity_for_target_buying_power(self, parameters):
        quantity = super().get_maximum_order_quantity_for_target_buying_power(parameters).quantity
        quantity = np.floor(quantity / 100) * 100
        return GetMaximumOrderQuantityResult(quantity)

    def has_sufficient_buying_power_for_order(self, parameters):
        return HasSufficientBuyingPowerForOrderResult(True)

    # Let's always return 0 as the maintenance margin so we avoid margin call orders
    def get_maintenance_margin(self, parameters):
        return MaintenanceMargin(0)

    # Override this as well because the base implementation calls GetMaintenanceMargin (overridden)
    # because in C# it wouldn't resolve the overridden Python method
    def get_reserved_buying_power_for_position(self, parameters):
        return parameters.result_in_account_currency(0)
