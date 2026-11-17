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
### Demostrates the use of <see cref="VolumeRenkoConsolidator"/> for creating constant volume bar
### </summary>
### <meta name="tag" content="renko" />
### <meta name="tag" content="using data" />
### <meta name="tag" content="consolidating data" />
class VolumeRenkoConsolidatorAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)

        self.sma = SimpleMovingAverage(10)
        self.tick_consolidated = False

        self.spy = self.add_equity("SPY", Resolution.MINUTE).symbol
        self.tradebar_volume_consolidator = VolumeRenkoConsolidator(1000000)
        self.tradebar_volume_consolidator.data_consolidated += self.on_spy_data_consolidated

        self.ibm = self.add_equity("IBM", Resolution.TICK).symbol
        self.tick_volume_consolidator = VolumeRenkoConsolidator(1000000)
        self.tick_volume_consolidator.data_consolidated += self.on_ibm_data_consolidated

        history = self.history[TradeBar](self.spy, 1000, Resolution.MINUTE)
        for bar in history:
            self.tradebar_volume_consolidator.update(bar)

    def on_spy_data_consolidated(self, sender, bar):
        self.sma.update(bar.end_time, bar.value)
        self.debug(f"SPY {bar.time} to {bar.end_time} :: O:{bar.open} H:{bar.high} L:{bar.low} C:{bar.close} V:{bar.volume}")
        if bar.volume != 1000000:
            raise Exception("Volume of consolidated bar does not match set value!")

    def on_ibm_data_consolidated(self, sender, bar):
        self.debug(f"IBM {bar.time} to {bar.end_time} :: O:{bar.open} H:{bar.high} L:{bar.low} C:{bar.close} V:{bar.volume}")
        if bar.volume != 1000000:
            raise Exception("Volume of consolidated bar does not match set value!")
        self.tick_consolidated = True

    def on_data(self, slice):
        # Update by TradeBar
        if slice.bars.contains_key(self.spy):
            self.tradebar_volume_consolidator.update(slice.bars[self.spy])

        # Update by Tick
        if slice.ticks.contains_key(self.ibm):
            for tick in slice.ticks[self.ibm]:
                self.tick_volume_consolidator.update(tick)

        if self.sma.is_ready and self.sma.current.value < self.securities[self.spy].price:
            self.set_holdings(self.spy, 1)
        else:
            self.set_holdings(self.spy, 0)
            
    def on_end_of_algorithm(self):
        if not self.tick_consolidated:
            raise Exception("Tick consolidator was never been called")