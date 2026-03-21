# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E42C0F2D
# Category: Indicators
# Description: Custom Indicator With Extension Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from collections import deque
from math import isclose

class CustomIndicatorWithExtensionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 9)
        self.set_end_date(2013, 10, 9)

        self.spy = self.add_equity("SPY", Resolution.MINUTE).symbol

        self.sma_values = []
        self.period = 10

        self.sma = self.sma(self.spy, self.period, Resolution.MINUTE)
        self.sma.updated += self.on_sma_updated

        self.custom_sma = CustomSimpleMovingAverage("My SMA", self.period)
        self.ext = IndicatorExtensions.of(self.custom_sma, self.sma)
        self.ext.updated += self.on_indicator_extension_updated

        self.sma_minus_custom = IndicatorExtensions.minus(self.sma, self.custom_sma)
        self.sma_minus_custom.updated += self.on_minus_updated

        self.sma_was_updated = False
        self.custom_sma_was_updated = False
        self.sma_minus_custom_was_updated = False

    def on_sma_updated(self, sender, updated):
        self.sma_was_updated = True

        if self.sma.is_ready:
            self.sma_values.append(self.sma.current.value)

    def on_indicator_extension_updated(self, sender, updated):
        self.custom_sma_was_updated = True

        sma_last_values = self.sma_values[-self.period:]
        expected = sum(sma_last_values) / len(sma_last_values)

        if not isclose(expected, self.custom_sma.value):
            raise Exception(f"Expected the custom SMA to calculate the moving average of the last {self.period} values of the SMA. "
                            f"Current expected: {expected}. Actual {self.custom_sma.value}.")

        self.debug(f"{self.sma.current.value} :: {self.custom_sma.value} :: {updated}")

    def on_minus_updated(self, sender, updated):
        self.sma_minus_custom_was_updated = True

        expected = self.sma.current.value - self.custom_sma.value

        if not isclose(expected, self.sma_minus_custom.current.value):
            raise Exception(f"Expected the composite minus indicator to calculate the difference between the SMA and custom SMA indicators. "
                            f"Expected: {expected}. Actual {self.sma_minus_custom.current.value}.")

    def on_end_of_algorithm(self):
        if not (self.sma_was_updated and self.custom_sma_was_updated and self.sma_minus_custom_was_updated):
            raise Exception("Expected all indicators to have been updated.")

# Custom indicator
class CustomSimpleMovingAverage(PythonIndicator):
    def __init__(self, name, period):
        self.name = name
        self.value = 0
        self.warm_up_period = period
        self.queue = deque(maxlen=period)

    def update(self, input: BaseData) -> bool:
        self.queue.appendleft(input.value)
        count = len(self.queue)
        self.value = sum(self.queue) / count

        return count == self.queue.maxlen

