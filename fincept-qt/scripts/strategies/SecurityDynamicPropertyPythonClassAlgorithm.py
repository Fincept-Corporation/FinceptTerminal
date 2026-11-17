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
