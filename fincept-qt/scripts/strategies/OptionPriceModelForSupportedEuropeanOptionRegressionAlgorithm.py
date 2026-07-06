# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-B6D1CB41
# Category: Options
# Description: Regression algorithm exercising an equity covered European style option, using an option price model that supports Eu...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from OptionPriceModelForOptionStylesBaseRegressionAlgorithm import OptionPriceModelForOptionStylesBaseRegressionAlgorithm

### <summary>
### Regression algorithm exercising an equity covered European style option, using an option price model
### that supports European style options and asserting that the option price model is used.
### </summary>
class OptionPriceModelForSupportedEuropeanOptionRegressionAlgorithm(OptionPriceModelForOptionStylesBaseRegressionAlgorithm):
    def initialize(self):
        self.set_start_date(2021, 1, 14)
        self.set_end_date(2021, 1, 14)

        option = self.add_index_option("SPX", Resolution.HOUR)
        # BlackScholes model supports European style options
        option.price_model = OptionPriceModels.black_scholes()

        self.set_warmup(7, Resolution.DAILY)

        self.init(option, option_style_is_supported=True)
