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
### Regression algorithm for testing that period-based history requests are not allowed with tick resolution
### </summary>
class PeriodBasedHistoryRequestNotAllowedWithTickResolutionRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 8)
        self.set_end_date(2013, 10, 9)

        spy = self.add_equity("SPY", Resolution.TICK).symbol

        # Tick resolution is not allowed for period-based history requests
        self.assert_that_history_throws_for_tick_resolution(lambda: self.history[Tick](spy, 1),
            "Tick history call with implicit tick resolution")
        self.assert_that_history_throws_for_tick_resolution(lambda: self.history[Tick](spy, 1, Resolution.TICK),
            "Tick history call with explicit tick resolution")
        self.assert_that_history_throws_for_tick_resolution(lambda: self.history[Tick]([spy], 1),
            "Tick history call with symbol array with implicit tick resolution")
        self.assert_that_history_throws_for_tick_resolution(lambda: self.history[Tick]([spy], 1, Resolution.TICK),
            "Tick history call with symbol array with explicit tick resolution")

        history = self.history[Tick](spy, TimeSpan.from_hours(12))
        if len(list(history)) == 0:
            raise Exception("On history call with implicit tick resolution: history returned no results")

        history = self.history[Tick](spy, TimeSpan.from_hours(12), Resolution.TICK)
        if len(list(history)) == 0:
            raise Exception("On history call with explicit tick resolution: history returned no results")

    def assert_that_history_throws_for_tick_resolution(self, history_call, history_call_description):
        try:
            history_call()
            raise Exception(f"{history_call_description}: expected an exception to be thrown")
        except InvalidOperationException:
            # expected
            pass
