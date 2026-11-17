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
# Modified by Fincept Corporation for the Fincept Terminal: comment text changed; the code is unchanged.

from AlgorithmImports import *

### <summary>
### Demonstration algorithm for the Warm Up feature with basic indicators.
### </summary>
### <meta name="tag" content="indicators" />
### <meta name="tag" content="warm up" />
### <meta name="tag" content="history and warm up" />
### <meta name="tag" content="using data" />
class WarmupAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,8)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY", Resolution.SECOND)

        fast_period = 60
        slow_period = 3600

        self.fast = self.EMA("SPY", fast_period)
        self.slow = self.EMA("SPY", slow_period)

        self.set_warmup(slow_period)
        self.first = True


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if self.first and not self.is_warming_up:
            self.first = False
            self.log("Fast: {0}".format(self.fast.samples))
            self.log("Slow: {0}".format(self.slow.samples))

        if self.fast.current.value > self.slow.current.value:
            self.set_holdings("SPY", 1)
        else:
            self.set_holdings("SPY", -1)
