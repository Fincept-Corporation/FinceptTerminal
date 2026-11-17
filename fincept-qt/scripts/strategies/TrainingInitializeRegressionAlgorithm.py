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
from time import sleep

### <summary>
### This regression algorithm is expected to fail and verifies that a training event
### created in Initialize will get run AND it will cause the algorithm to fail if it
### exceeds the "algorithm-manager-time-loop-maximum" config value, which the regression
### test sets to 0.5 minutes.
### </summary>
class TrainingInitializeRegressionAlgorithm(QCAlgorithm):
    '''Example algorithm showing how to use QCAlgorithm.train method'''

    def initialize(self):

        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

        self.add_equity("SPY", Resolution.DAILY)

        # this should cause the algorithm to fail
        # the regression test sets the time limit to 30 seconds and there's one extra
        # minute in the bucket, so a two minute sleep should result in RuntimeError
        self.train(lambda: sleep(150))

        # DateRules.tomorrow combined with TimeRules.midnight enforces that this event schedule will
        # have exactly one time, which will fire between the first data point and the next day at
        # midnight. So after the first data point, it will run this event and sleep long enough to
        # exceed the static max algorithm time loop time and begin to consume from the leaky bucket
        # the regression test sets the "algorithm-manager-time-loop-maximum" value to 30 seconds
        self.train(self.date_rules.tomorrow, self.time_rules.midnight, lambda: sleep(60))
                    # this will consume the single 'minute' available in the leaky bucket
                    # and the regression test will confirm that the leaky bucket is empty
