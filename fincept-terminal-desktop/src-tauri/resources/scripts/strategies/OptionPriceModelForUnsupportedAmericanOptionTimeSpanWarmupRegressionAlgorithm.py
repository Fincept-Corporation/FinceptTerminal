# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-850F1B89
# Category: Options
# Description: Regression algorithm exercising an equity covered American style option, using an option price model that supports Am...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from OptionPriceModelForUnsupportedAmericanOptionRegressionAlgorithm import OptionPriceModelForUnsupportedAmericanOptionRegressionAlgorithm

### <summary>
### Regression algorithm exercising an equity covered American style option, using an option price model
### that supports American style options and asserting that the option price model is used.
### </summary>
class OptionPriceModelForUnsupportedAmericanOptionTimeSpanWarmupRegressionAlgorithm(OptionPriceModelForUnsupportedAmericanOptionRegressionAlgorithm):
    def initialize(self):
        OptionPriceModelForUnsupportedAmericanOptionRegressionAlgorithm.initialize(self)

        # We want to match the start time of the base algorithm: Base algorithm warmup is 2 bar of daily resolution.
        # So to match the same start time we go back 4 days, we need to account for a single weekend. This is calculated by 'Time.GET_START_TIME_FOR_TRADE_BARS'
        self.set_warmup(TimeSpan.from_days(4))
