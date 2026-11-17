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
### Regression algorithm asserting that tick history request includes both trade and quote data
### </summary>
class HistoryTickRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 11)
        self.set_end_date(2013, 10, 11)

        self._symbol = self.add_equity("SPY", Resolution.TICK).symbol

    def on_end_of_algorithm(self):
        history = list(self.history[Tick](self._symbol, timedelta(days=1), Resolution.TICK))
        quotes = [x for x in history if x.tick_type == TickType.QUOTE]
        trades = [x for x in history if x.tick_type == TickType.TRADE]

        if not quotes or not trades:
            raise Exception("Expected to find at least one tick of each type (quote and trade)")
