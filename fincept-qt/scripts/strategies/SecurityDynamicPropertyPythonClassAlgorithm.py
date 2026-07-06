# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-B2BE2584
# Category: General Strategy
# Description: Algorithm asserting that security dynamic properties keep Python references to the Python class they are instances of...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from collections import deque

### <summary>
### Algorithm asserting that security dynamic properties keep Python references to the Python class they are instances of,
### specifically when this class is a subclass of a C# class.
### </summary>
class SecurityDynamicPropertyPythonClassAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 7)

        self.spy = self.add_equity("SPY", Resolution.MINUTE)

        custom_sma = CustomSimpleMovingAverage('custom', 60)
        self.spy.custom_sma = custom_sma
        custom_sma.security = self.spy

        self.register_indicator(self.spy.symbol, self.spy.custom_sma,  Resolution.MINUTE)

    def on_warmup_finished(self) -> None:
        if type(self.spy.custom_sma) != CustomSimpleMovingAverage:
            raise Exception("spy.custom_sma is not an instance of CustomSimpleMovingAverage")

        if self.spy.custom_sma.security is None:
            raise Exception("spy.custom_sma.security is None")
        else:
            self.debug(f"spy.custom_sma.security.symbol: {self.spy.custom_sma.security.symbol}")

    def on_data(self, slice: Slice) -> None:
        if self.spy.custom_sma.is_ready:
            self.debug(f"CustomSMA: {self.spy.custom_sma.current.value}")

class CustomSimpleMovingAverage(PythonIndicator):
    def __init__(self, name, period):
        super().__init__()
        self.name = name
        self.value = 0
        self.queue = deque(maxlen=period)

    def update(self, input):
        self.queue.appendleft(input.value)
        count = len(self.queue)
        self.value = np.sum(self.queue) / count
        return count == self.queue.maxlen
