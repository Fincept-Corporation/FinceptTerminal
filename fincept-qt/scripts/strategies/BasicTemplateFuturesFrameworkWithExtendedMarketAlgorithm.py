# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-456D112C
# Category: Futures
# Description: Basic template futures framework algorithm uses framework components to define an algorithm that trades futures
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from BasicTemplateFuturesFrameworkAlgorithm import BasicTemplateFuturesFrameworkAlgorithm

### <summary>
### Basic template futures framework algorithm uses framework components
### to define an algorithm that trades futures.
### </summary>
class BasicTemplateFuturesFrameworkWithExtendedMarketAlgorithm(BasicTemplateFuturesFrameworkAlgorithm):

    def get_extended_market_hours(self):
        return True
