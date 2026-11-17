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

from BasicTemplateFuturesFrameworkAlgorithm import BasicTemplateFuturesFrameworkAlgorithm

### <summary>
### Basic template futures framework algorithm uses framework components
### to define an algorithm that trades futures.
### </summary>
class BasicTemplateFuturesFrameworkWithExtendedMarketAlgorithm(BasicTemplateFuturesFrameworkAlgorithm):

    def get_extended_market_hours(self):
        return True
