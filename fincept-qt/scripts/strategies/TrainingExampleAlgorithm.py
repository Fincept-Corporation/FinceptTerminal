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
### Example algorithm showing how to use QCAlgorithm.train method
### </summary>
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="training" />
class TrainingExampleAlgorithm(QCAlgorithm):
    '''Example algorithm showing how to use QCAlgorithm.train method'''

    def initialize(self):

        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 14)

        self.add_equity("SPY", Resolution.DAILY)

        # Set TrainingMethod to be executed immediately
        self.train(self.training_method)

        # Set TrainingMethod to be executed at 8:00 am every Sunday
        self.train(self.date_rules.every(DayOfWeek.SUNDAY), self.time_rules.at(8 , 0), self.training_method)

    def training_method(self):

        self.log(f'Start training at {self.time}')
        # Use the historical data to train the machine learning model
        history = self.history(["SPY"], 200, Resolution.DAILY)

        # ML code:
        pass
