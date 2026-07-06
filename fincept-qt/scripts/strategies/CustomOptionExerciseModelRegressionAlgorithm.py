# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A7E25D46
# Category: Options
# Description: Regression algorithm asserting we can specify a custom option exercise model
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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
