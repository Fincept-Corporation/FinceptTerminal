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

from datetime import timedelta
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting that historical data can be requested with tick resolution without requiring
### a tick resolution subscription
### </summary>
class TickHistoryRequestWithoutTickSubscriptionRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 8)
        self.set_end_date(2013, 10, 8)

        # Subscribing SPY and IBM with daily and hour resolution instead of tick resolution
        spy = self.add_equity("SPY", Resolution.DAILY).symbol
        ibm = self.add_equity("IBM", Resolution.HOUR).symbol

        # Requesting history for SPY and IBM (separately) with tick resolution
        spy_history = self.history[Tick](spy, timedelta(days=1), Resolution.TICK)
        if len(list(spy_history)) == 0:
            raise Exception("SPY tick history is empty")

        ibm_history = self.history[Tick](ibm, timedelta(days=1), Resolution.TICK)
        if len(list(ibm_history)) == 0:
            raise Exception("IBM tick history is empty")

        # Requesting history for SPY and IBM (together) with tick resolution
        spy_ibm_history = self.history[Tick]([spy, ibm], timedelta(days=1), Resolution.TICK)
        if len(list(spy_ibm_history)) == 0:
            raise Exception("Compound SPY and IBM tick history is empty")

        self.quit()
