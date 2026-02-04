# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-253853F6
# Category: Data Consolidation
# Description: Demostrates the use of <see cref="VolumeRenkoConsolidator"/> for creating constant volume bar
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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