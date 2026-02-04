# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D909F7DF
# Category: Data Consolidation
# Description: Demonstration of how to initialize and use the Classic RenkoConsolidator
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of how to initialize and use the Classic RenkoConsolidator
### </summary>
### <meta name="tag" content="renko" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="using data" />
### <meta name="tag" content="consolidating data" />
class ClassicRenkoConsolidatorAlgorithm(QCAlgorithm):
    '''Demonstration of how to initialize and use the RenkoConsolidator'''

    def initialize(self):

        self.set_start_date(2012, 1, 1)
        self.set_end_date(2013, 1, 1)

        self.add_equity("SPY", Resolution.DAILY)

        # this is the simple constructor that will perform the
        # renko logic to the Value property of the data it receives.

        # break SPY into $2.5 renko bricks and send that data to our 'OnRenkoBar' method
        renko_close = ClassicRenkoConsolidator(2.5)
        renko_close.data_consolidated += self.handle_renko_close
        self.subscription_manager.add_consolidator("SPY", renko_close)

        # this is the full constructor that can accept a value selector and a volume selector
        # this allows us to perform the renko logic on values other than Close, even computed values!

        # break SPY into (2*o + h + l + 3*c)/7
        renko7bar = ClassicRenkoConsolidator(2.5, lambda x: (2 * x.open + x.high + x.low + 3 * x.close) / 7, lambda x: x.volume)
        renko7bar.data_consolidated += self.handle_renko7_bar
        self.subscription_manager.add_consolidator("SPY", renko7bar)


    # We're doing our analysis in the on_renko_bar method, but the framework verifies that this method exists, so we define it.
    def on_data(self, data):
        pass


    def handle_renko_close(self, sender, data):
        '''This function is called by our renko_close consolidator defined in Initialize()
        Args:
            data: The new renko bar produced by the consolidator'''
        if not self.portfolio.invested:
            self.set_holdings(data.symbol, 1)

        self.log(f"CLOSE - {data.time} - {data.open} {data.close}")


    def handle_renko7_bar(self, sender, data):
        '''This function is called by our renko7bar consolidator defined in Initialize()
        Args:
            data: The new renko bar produced by the consolidator'''
        if self.portfolio.invested:
            self.liquidate(data.symbol)
        self.log(f"7BAR - {data.time} - {data.open} {data.close}")
