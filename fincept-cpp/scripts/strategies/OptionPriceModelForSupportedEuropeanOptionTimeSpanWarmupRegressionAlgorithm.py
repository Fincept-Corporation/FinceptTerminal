# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-6812E1E8
# Category: Options
# Description: Regression algorithm exercising an equity covered European style option, using an option price model that supports Eu...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from OptionPriceModelForSupportedEuropeanOptionRegressionAlgorithm import OptionPriceModelForSupportedEuropeanOptionRegressionAlgorithm

### <summary>
### Regression algorithm exercising an equity covered European style option, using an option price model
### that supports European style options and asserting that the option price model is used.
### </summary>
class OptionPriceModelForSupportedEuropeanOptionTimeSpanWarmupRegressionAlgorithm(OptionPriceModelForSupportedEuropeanOptionRegressionAlgorithm):
    def initialize(self):
        OptionPriceModelForSupportedEuropeanOptionRegressionAlgorithm.initialize(self)

        # We want to match the start time of the base algorithm. SPX index options data time zone is chicago, algorithm time zone is new york (default).
        # Base algorithm warmup is 7 bar of daily resolution starts at 23 PM new york time of T-1. So to match the same start time
        # we go back a 9 day + 23 hours, we need to account for a single weekend. This is calculated by 'Time.GET_START_TIME_FOR_TRADE_BARS'
        self.set_warmup(TimeSpan.from_hours(24 * 9 + 23))
