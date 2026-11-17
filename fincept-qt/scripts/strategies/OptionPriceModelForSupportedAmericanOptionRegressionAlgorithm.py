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
from OptionPriceModelForOptionStylesBaseRegressionAlgorithm import OptionPriceModelForOptionStylesBaseRegressionAlgorithm

### <summary>
### Regression algorithm exercising an equity covered American style option, using an option price model
### that supports American style options and asserting that the option price model is used.
### </summary>
class OptionPriceModelForSupportedAmericanOptionRegressionAlgorithm(OptionPriceModelForOptionStylesBaseRegressionAlgorithm):
    def initialize(self):
        self.set_start_date(2014, 6, 9)
        self.set_end_date(2014, 6, 9)

        option = self.add_option("AAPL", Resolution.MINUTE)
        # BaroneAdesiWhaley model supports American style options
        option.price_model = OptionPriceModels.barone_adesi_whaley()

        self.set_warmup(2, Resolution.DAILY)

        self.init(option, option_style_is_supported=True)
