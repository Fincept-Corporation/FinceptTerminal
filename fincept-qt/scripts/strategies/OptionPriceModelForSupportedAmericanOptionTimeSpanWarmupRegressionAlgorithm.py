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
from OptionPriceModelForSupportedAmericanOptionRegressionAlgorithm import OptionPriceModelForSupportedAmericanOptionRegressionAlgorithm

### <summary>
### Regression algorithm exercising an equity covered American style option, using an option price model
### that supports American style options and asserting that the option price model is used.
### </summary>
class OptionPriceModelForSupportedAmericanOptionTimeSpanWarmupRegressionAlgorithm(OptionPriceModelForSupportedAmericanOptionRegressionAlgorithm):
    def initialize(self):
        OptionPriceModelForSupportedAmericanOptionRegressionAlgorithm.initialize(self)

        # We want to match the start time of the base algorithm: Base algorithm warmup is 2 bar of daily resolution.
        # So to match the same start time we go back 4 days, we need to account for a single weekend. This is calculated by 'Time.GET_START_TIME_FOR_TRADE_BARS'
        self.set_warmup(TimeSpan.from_days(4))
