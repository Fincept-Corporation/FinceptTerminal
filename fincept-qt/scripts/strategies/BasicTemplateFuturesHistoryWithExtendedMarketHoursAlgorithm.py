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
