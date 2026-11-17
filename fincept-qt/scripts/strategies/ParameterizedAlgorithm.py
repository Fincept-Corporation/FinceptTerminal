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
### Demonstration of the parameter system of QuantConnect. Using parameters you can pass the values required into C# algorithms for optimization.
### </summary>
### <meta name="tag" content="optimization" />
### <meta name="tag" content="using quantconnect" />
class ParameterizedAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013, 10, 7)   #Set Start Date
        self.set_end_date(2013, 10, 11)    #Set End Date
        self.set_cash(100000)             #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY")

        # Receive parameters from the Job
        fast_period = self.get_parameter("ema-fast", 100)
        slow_period = self.get_parameter("ema-slow", 200)

        self.fast = self.ema("SPY", fast_period)
        self.slow = self.ema("SPY", slow_period)


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''

        # wait for our indicators to ready
        if not self.fast.is_ready or not self.slow.is_ready:
            return

        fast = self.fast.current.value
        slow = self.slow.current.value

        if fast > slow * 1.001:
            self.set_holdings("SPY", 1)
        elif fast < slow * 0.999:
            self.liquidate("SPY")
