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
