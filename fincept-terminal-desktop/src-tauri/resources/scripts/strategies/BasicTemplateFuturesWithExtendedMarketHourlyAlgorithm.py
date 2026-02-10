# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-006DF51F
# Category: Futures
# Description: This example demonstrates how to add futures with hourly resolution and extended market hours
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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

from AlgorithmImports import *
from BasicTemplateFuturesHourlyAlgorithm import BasicTemplateFuturesHourlyAlgorithm

### <summary>
### This example demonstrates how to add futures with hourly resolution and extended market hours.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="benchmarks" />
### <meta name="tag" content="futures" />
class BasicTemplateFuturesWithExtendedMarketHourlyAlgorithm(BasicTemplateFuturesHourlyAlgorithm):

    def get_extended_market_hours(self):
        return True
