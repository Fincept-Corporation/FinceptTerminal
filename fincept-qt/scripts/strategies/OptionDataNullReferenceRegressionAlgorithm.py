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
### This algorithm is a regression test for issue #2018 and PR #2038.
### </summary>
class OptionDataNullReferenceRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2016, 12, 1)
        self.set_end_date(2017, 1, 1)
        self.set_cash(500000)

        self.add_equity("DUST")

        option = self.add_option("DUST")

        option.set_filter(self.universe_func)

    def universe_func(self, universe):
        return universe.include_weeklys().strikes(-1, +1).expiration(timedelta(25), timedelta(100))
