# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C99DFC04
# Category: Options
# Description: Regression algorithm exercising an equity covered European style option, using an option price model that does not su...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from OptionPriceModelForOptionStylesBaseRegressionAlgorithm import OptionPriceModelForOptionStylesBaseRegressionAlgorithm

### <summary>
### Regression algorithm exercising an equity covered European style option, using an option price model
### that does not support European style options and asserting that the option price model is not used.
### </summary>
class OptionPriceModelForUnsupportedEuropeanOptionRegressionAlgorithm(OptionPriceModelForOptionStylesBaseRegressionAlgorithm):
    def initialize(self):
        self.set_start_date(2021, 1, 14)
        self.set_end_date(2021, 1, 14)

        option = self.add_index_option("SPX", Resolution.HOUR)
        # BaroneAdesiWhaley model does not support European style options
        option.price_model = OptionPriceModels.barone_adesi_whaley()

        self.set_warmup(7, Resolution.DAILY)

        self.init(option, option_style_is_supported=False)
