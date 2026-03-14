# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-62B73DDD
# Category: Options
# Description: Algorithm asserting that when setting custom models for canonical securities, a one-time warning is sent informing th...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from System import Action

from QuantConnect.Logging import *

### <summary>
### Algorithm asserting that when setting custom models for canonical securities, a one-time warning is sent
### informing the user that the contracts models are different (not the custom ones).
### </summary>
class OptionModelsConsistencyRegressionAlgorithm(QCAlgorithm):

    def initialize(self) -> None:
        security = self.initialize_algorithm()
        self.set_models(security)

        # Using a custom security initializer derived from BrokerageModelSecurityInitializer
        # to check that the models are correctly set in the security even when the
        # security initializer is derived from said class in Python
        self.set_security_initializer(CustomSecurityInitializer(self.brokerage_model, SecuritySeeder.NULL))

        self.set_benchmark(lambda x: 0)

    def initialize_algorithm(self) -> Security:
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)

        equity = self.add_equity("GOOG", leverage=4)
        option = self.add_option(equity.symbol)
        option.set_filter(lambda u: u.strikes(-2, +2).expiration(0, 180))

        return option

    def set_models(self, security: Security) -> None:
        security.set_fill_model(CustomFillModel())
        security.set_fee_model(CustomFeeModel())
        security.set_buying_power_model(CustomBuyingPowerModel())
        security.set_slippage_model(CustomSlippageModel())
        security.set_volatility_model(CustomVolatilityModel())

class CustomSecurityInitializer(BrokerageModelSecurityInitializer):
    def __init__(self, brokerage_model: BrokerageModel, security_seeder: SecuritySeeder):
        super().__init__(brokerage_model, security_seeder)

class CustomFillModel(FillModel):
    pass

class CustomFeeModel(FeeModel):
    pass

class CustomBuyingPowerModel(BuyingPowerModel):
    pass

class CustomSlippageModel(ConstantSlippageModel):
    def __init__(self):
        super().__init__(0)

class CustomVolatilityModel(BaseVolatilityModel):
    pass
