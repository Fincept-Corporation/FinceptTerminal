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
### Constructs a displaced moving average ribbon and buys when all are lined up, liquidates when they all line down
### Ribbons are great for visualizing trends
### Signals are generated when they all line up in a paricular direction
### A buy signal is when the values of the indicators are increasing (from slowest to fastest).
### A sell signal is when the values of the indicators are decreasing (from slowest to fastest).
### </summary>
### <meta name="tag" content="charting" />
### <meta name="tag" content="plotting indicators" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="indicator classes" />
class DisplacedMovingAverageRibbon(QCAlgorithm):

    # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    def initialize(self):
        self.set_start_date(2009, 1, 1)  #Set Start Date
        self.set_end_date(2015, 1, 1)    #Set End Date
        self.spy = self.add_equity("SPY", Resolution.DAILY).symbol
        count = 6
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
        self.previous = datetime.min
        # plot indicators each time they update using the PlotIndicator function
        for i in self.ribbon:
            self.plot_indicator("Ribbon", i) 

    # on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    def on_data(self, data):
        
        if data[self.spy] is None: return
        # wait for our entire ribbon to be ready
        if not all(x.is_ready for x in self.ribbon): return
        # only once per day
        if self.previous.date() == self.time.date(): return
        self.plot("Ribbon", "Price", data[self.spy].price)

        # check for a buy signal
        values = [x.current.value for x in self.ribbon]
        holding = self.portfolio[self.spy]
        if (holding.quantity <= 0 and self.is_ascending(values)):
            self.set_holdings(self.spy, 1.0)
        elif (holding.quantity > 0 and self.is_descending(values)):
            self.liquidate(self.spy)
        self.previous = self.time
    
    # Returns true if the specified values are in ascending order
    def is_ascending(self, values):
        last = None
        for val in values:
            if last is None:
                last = val
                continue
            if last < val:
                return False
            last = val
        return True
    
    # Returns true if the specified values are in Descending order
    def is_descending(self, values):
        last = None
        for val in values:
            if last is None:
                last = val
                continue
            if last > val:
                return False
            last = val
        return True
