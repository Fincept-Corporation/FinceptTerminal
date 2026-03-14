# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9713D1F5
# Category: Futures
# Description: This example demonstrates how to get access to futures history for a given root symbol with extended market hours. It...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from BasicTemplateFuturesHistoryAlgorithm import BasicTemplateFuturesHistoryAlgorithm

### <summary>
### This example demonstrates how to get access to futures history for a given root symbol with extended market hours.
### It also shows how you can prefilter contracts easily based on expirations, and inspect the futures
### chain to pick a specific contract to trade.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="history and warm up" />
### <meta name="tag" content="history" />
### <meta name="tag" content="futures" />
class BasicTemplateFuturesHistoryWithExtendedMarketHoursAlgorithm(BasicTemplateFuturesHistoryAlgorithm):

    def get_extended_market_hours(self):
        return True

    def get_expected_history_call_count(self):
        return 49
