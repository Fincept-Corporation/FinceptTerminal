# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-03E734EA
# Category: Options
# Description: Regression algorithm exercising an equity covered American style option, using an option price model that supports Am...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from OptionPriceModelForOptionStylesBaseRegressionAlgorithm import OptionPriceModelForOptionStylesBaseRegressionAlgorithm

### <summary>
### Regression algorithm exercising an equity covered American style option, using an option price model
### that supports American style options and asserting that the option price model is used.
### </summary>
class OptionPriceModelForUnsupportedAmericanOptionRegressionAlgorithm(OptionPriceModelForOptionStylesBaseRegressionAlgorithm):
    def initialize(self):
        self.set_start_date(2014, 6, 9)
        self.set_end_date(2014, 6, 9)

        option = self.add_option("AAPL", Resolution.MINUTE)
        # BlackSholes model does not support American style options
        option.price_model = OptionPriceModels.black_scholes()

        self.set_warmup(2, Resolution.DAILY)

        self.init(option, option_style_is_supported=False)
