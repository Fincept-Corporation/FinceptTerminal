# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1D8349B6
# Category: Options
# Description: Index Option Bull Put Spread Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class IndexOptionBullPutSpreadAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2019, 1, 1)
        self.set_end_date(2020, 1, 1)
        self.set_cash(100000)

        index = self.add_index("SPX", Resolution.MINUTE).symbol
        option = self.add_index_option(index, "SPXW", Resolution.MINUTE)
        option.set_filter(lambda x: x.weeklys_only().strikes(-10, -5).expiration(0, 0))
        
        self.spxw = option.symbol
        self.tickets = []

    def on_data(self, slice: Slice) -> None:
        # Return if open position exists
        if any([self.portfolio[x.symbol].invested for x in self.tickets]):
            return

        # Get option chain
        chain = slice.option_chains.get(self.spxw)
        if not chain: return

        # Get the nearest expiry date of the contracts
        expiry = min([x.expiry for x in chain])
        
        # Select the put Option contracts with the nearest expiry and sort by strike price
        puts = sorted([i for i in chain if i.expiry == expiry and i.right == OptionRight.PUT], 
                        key=lambda x: x.strike)
        if len(puts) < 2: return

        # Buy the bull put spread
        bull_call_spread = OptionStrategies.bull_put_spread(self.spxw, puts[-1].strike, puts[0].strike, expiry)
        self.tickets = self.buy(bull_call_spread, 1)