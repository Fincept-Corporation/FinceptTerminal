# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-223472B2
# Category: Indicators
# Description: Regression test to check python indicator is keeping backwards compatibility  with indicators that do not set WarmUpP...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from collections import deque

### <summary>
### Regression test to check python indicator is keeping backwards compatibility 
### with indicators that do not set WarmUpPeriod or do not inherit from PythonIndicator class.
### </summary>
### <meta name="tag" content="indicators" />
### <meta name="tag" content="indicator classes" />
### <meta name="tag" content="custom indicator" />
### <meta name="tag" content="regression test" />
class CustomWarmUpPeriodIndicatorAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)
        self.add_equity("SPY", Resolution.SECOND)

        # Create three python indicators
        # - custom_not_warm_up does not define WarmUpPeriod parameter
        # - custom_warm_up defines WarmUpPeriod parameter
        # - custom_not_inherit defines WarmUpPeriod parameter but does not inherit from PythonIndicator class
        # - csharp_indicator defines WarmUpPeriod parameter and represents the traditional LEAN C# indicator
        self.custom_not_warm_up = CSMANotWarmUp('custom_not_warm_up', 60)
        self.custom_warm_up = CSMAWithWarmUp('custom_warm_up', 60)
        self.custom_not_inherit = CustomSMA('custom_not_inherit', 60)
        self.csharp_indicator = SimpleMovingAverage('csharp_indicator', 60)

        # Register the daily data of "SPY" to automatically update the indicators
        self.register_indicator("SPY", self.custom_warm_up, Resolution.MINUTE)
        self.register_indicator("SPY", self.custom_not_warm_up, Resolution.MINUTE)
        self.register_indicator("SPY", self.custom_not_inherit, Resolution.MINUTE)
        self.register_indicator("SPY", self.csharp_indicator, Resolution.MINUTE)

        # Warm up custom_warm_up indicator
        self.warm_up_indicator("SPY", self.custom_warm_up, Resolution.MINUTE)

        # Check custom_warm_up indicator has already been warmed up with the requested data
        assert(self.custom_warm_up.is_ready), "custom_warm_up indicator was expected to be ready"
        assert(self.custom_warm_up.samples == 60), "custom_warm_up indicator was expected to have processed 60 datapoints already"

        # Try to warm up custom_not_warm_up indicator. It's expected from LEAN to skip the warm up process
        # because this indicator doesn't define WarmUpPeriod parameter
        self.warm_up_indicator("SPY", self.custom_not_warm_up, Resolution.MINUTE)

        # Check custom_not_warm_up indicator is not ready and is using the default WarmUpPeriod value
        assert(not self.custom_not_warm_up.is_ready), "custom_not_warm_up indicator wasn't expected to be warmed up"
        assert(self.custom_not_warm_up.warm_up_period == 0), "custom_not_warm_up indicator WarmUpPeriod parameter was expected to be 0"

        # Warm up custom_not_inherit indicator. Though it does not inherit from PythonIndicator class,
        # it defines WarmUpPeriod parameter so it's expected to be warmed up from LEAN
        self.warm_up_indicator("SPY", self.custom_not_inherit, Resolution.MINUTE)

        # Check custom_not_inherit indicator has already been warmed up with the requested data
        assert(self.custom_not_inherit.is_ready), "custom_not_inherit indicator was expected to be ready"
        assert(self.custom_not_inherit.samples == 60), "custom_not_inherit indicator was expected to have processed 60 datapoints already"

        # Warm up csharp_indicator
        self.warm_up_indicator("SPY", self.csharp_indicator, Resolution.MINUTE)

        # Check csharp_indicator indicator has already been warmed up with the requested data
        assert(self.csharp_indicator.is_ready), "csharp_indicator indicator was expected to be ready"
        assert(self.csharp_indicator.samples == 60), "csharp_indicator indicator was expected to have processed 60 datapoints already"

    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)

        if self.time.second == 0:
            # Compute the difference between indicators values
            diff = abs(self.custom_not_warm_up.current.value - self.custom_warm_up.current.value)
            diff += abs(self.custom_not_inherit.value - self.custom_not_warm_up.current.value)
            diff += abs(self.custom_not_inherit.value - self.custom_warm_up.current.value)
            diff += abs(self.csharp_indicator.current.value - self.custom_warm_up.current.value)
            diff += abs(self.csharp_indicator.current.value - self.custom_not_warm_up.current.value)
            diff += abs(self.csharp_indicator.current.value - self.custom_not_inherit.value)

            # Check custom_not_warm_up indicator is ready when the number of samples is bigger than its WarmUpPeriod parameter
            assert(self.custom_not_warm_up.is_ready == (self.custom_not_warm_up.samples >= 60)), "custom_not_warm_up indicator was expected to be ready when the number of samples were bigger that its WarmUpPeriod parameter"

            # Check their values are the same. We only need to check if custom_not_warm_up indicator is ready because the other ones has already been asserted to be ready
            assert(diff <= 1e-10 or (not self.custom_not_warm_up.is_ready)), f"The values of the indicators are not the same. Indicators difference is {diff}"
            
# Python implementation of SimpleMovingAverage.
# Represents the traditional simple moving average indicator (SMA) without Warm Up Period parameter defined
class CSMANotWarmUp(PythonIndicator):
    def __init__(self, name, period):
        super().__init__()
        self.name = name
        self.value = 0
        self.queue = deque(maxlen=period)

    # Update method is mandatory
    def update(self, input):
        self.queue.appendleft(input.value)
        count = len(self.queue)
        self.value = np.sum(self.queue) / count
        return count == self.queue.maxlen

# Python implementation of SimpleMovingAverage.
# Represents the traditional simple moving average indicator (SMA) With Warm Up Period parameter defined
class CSMAWithWarmUp(CSMANotWarmUp):
    def __init__(self, name, period):
        super().__init__(name, period)
        self.warm_up_period = period

# Custom python implementation of SimpleMovingAverage.
# Represents the traditional simple moving average indicator (SMA)
class CustomSMA():
    def __init__(self, name, period):
        self.name = name
        self.value = 0
        self.queue = deque(maxlen=period)
        self.warm_up_period = period
        self.is_ready = False
        self.samples = 0

    # Update method is mandatory
    def update(self, input):
        self.samples += 1
        self.queue.appendleft(input.value)
        count = len(self.queue)
        self.value = np.sum(self.queue) / count
        if count == self.queue.maxlen:
            self.is_ready = True
        return self.is_ready
