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
from QuantConnect.Algorithm.CSharp import *

### <summary>
### Regression algorithm asserting we can specify a custom option exercise model
### </summary>
class CustomOptionExerciseModelRegressionAlgorithm(OptionAssignmentRegressionAlgorithm):
    def initialize(self):
        self.set_security_initializer(self.custom_security_initializer)
        super().initialize()

    def custom_security_initializer(self, security):
        if Extensions.is_option(security.symbol.security_type):
            security.set_option_exercise_model(CustomExerciseModel())

    def on_data(self, data):
        super().on_data(data)

class CustomExerciseModel(DefaultExerciseModel):
    def option_exercise(self, option: Option, order: OptionExerciseOrder):
        order_event = OrderEvent(
            order.id,
            option.symbol,
            Extensions.convert_to_utc(option.local_time, option.exchange.time_zone),
            OrderStatus.FILLED,
            Extensions.get_order_direction(order.quantity),
            0.0,
            order.quantity,
            OrderFee.ZERO,
            "Tag"
        )
        order_event.is_assignment = False
        return [ order_event ]
