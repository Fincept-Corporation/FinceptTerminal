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

### <summary>
### Benchmark Algorithm: Pure processing of 1 equity second resolution with the same benchmark.
### </summary>
### <remarks>
### This should eliminate the synchronization part of LEAN and focus on measuring the performance of a single datafeed and event handling system.
### </remarks>
class EmptySingleSecuritySecondEquityBenchmark(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2008, 1, 1)
        self.set_end_date(2008, 6, 1)
        self.set_benchmark(lambda x: 1)
        self.add_equity("SPY", Resolution.SECOND)
    
    def on_data(self, data):
        pass
