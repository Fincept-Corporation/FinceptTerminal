# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-84B15249
# Category: General Strategy
# Description: Tick Data Filtering Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

# <summary>
# Demonstration of filtering tick data so easier to use. Tick data has lots of glitchy, spikey data which should be filtered out before usagee.
# </summary>
# <meta name="tag" content="filtering" />
# <meta name="tag" content="tick data" />
# <meta name="tag" content="using data" />
# <meta name="tag" content="ticks event" />
class TickDataFilteringAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 7)
        self.set_cash(25000)
        spy = self.add_equity("SPY", Resolution.TICK)

        #Add our custom data filter.
        spy.set_data_filter(TickExchangeDataFilter(self))

    # <summary>
    # Data arriving here will now be filtered.
    # </summary>
    # <param name="data">Ticks data array</param>
    def on_data(self, data):
        if not data.contains_key("SPY"): 
            return
        
        spy_tick_list = data["SPY"]

        # Ticks return a list of ticks this second
        for tick in spy_tick_list:
            self.debug(tick.exchange)

        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)

# <summary>
# Exchange filter class
# </summary>
class TickExchangeDataFilter(SecurityDataFilter):

    # <summary>
    # Save instance of the algorithm namespace
    # </summary>
    # <param name="algo"></param>
    def __init__(self, algo: IAlgorithm):
        self.algo = algo
        super().__init__()

    # <summary>
    # Filter out a tick from this vehicle, with this new data:
    # </summary>
    # <param name="data">New data packet:</param>
    # <param name="asset">Vehicle of this filter.</param>
    def filter(self, asset: Security, data: BaseData):
        # TRUE -->  Accept Tick
        # FALSE --> Reject Tick

        if isinstance(data, Tick):
            if data.exchange == str(Exchange.ARCA):
                return True
        
        return False
