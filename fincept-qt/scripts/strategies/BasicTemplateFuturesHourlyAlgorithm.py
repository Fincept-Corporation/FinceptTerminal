# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D52E449D
# Category: Futures
# Description: This example demonstrates how to add futures with hourly resolution
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from BasicTemplateFuturesDailyAlgorithm import BasicTemplateFuturesDailyAlgorithm

### <summary>
### This example demonstrates how to add futures with hourly resolution.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="benchmarks" />
### <meta name="tag" content="futures" />
class BasicTemplateFuturesHourlyAlgorithm(BasicTemplateFuturesDailyAlgorithm):

    def get_resolution(self):
        return Resolution.HOUR
