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

class IndicatorRibbonBenchmark(QCAlgorithm):

    # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    def initialize(self):
        self.set_start_date(2010, 1, 1)  #Set Start Date
        self.set_end_date(2018, 1, 1)    #Set End Date
        self.spy = self.add_equity("SPY", Resolution.MINUTE).symbol
        count = 50
        offset = 5
        period = 15
        self.ribbon = []
        # define our sma as the base of the ribbon
        self.sma = SimpleMovingAverage(period)
        
        for x in range(count):
            # define our offset to the zero sma, these various offsets will create our 'displaced' ribbon
            delay = Delay(offset*(x+1))
            # define an indicator that takes the output of the sma and pipes it into our delay indicator
            delayed_sma = IndicatorExtensions.of(delay, self.sma)
            # register our new 'delayed_sma' for automatic updates on a daily resolution
            self.register_indicator(self.spy, delayed_sma, Resolution.DAILY)
            self.ribbon.append(delayed_sma)

    def on_data(self, data):
        # wait for our entire ribbon to be ready
        if not all(x.is_ready for x in self.ribbon): return
        for x in self.ribbon:
            value = x.current.value
