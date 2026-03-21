# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-82E65BB4
# Category: Options
# Description: Index Option Bull Call Spread Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
#endregion

class IndexOptionBullCallSpreadAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 1, 1)
        self.set_end_date(2021, 1, 1)
        self.set_cash(100000)

        self.spy = self.add_equity("SPY", Resolution.MINUTE).symbol

        index = self.add_index("SPX", Resolution.MINUTE).symbol
        option = self.add_index_option(index, "SPXW", Resolution.MINUTE)
        option.set_filter(lambda x: x.weeklys_only().strikes(-5, 5).expiration(40, 60))
        
        self.spxw = option.symbol
        self.tickets = []

    def on_data(self, slice: Slice) -> None:
        if not self.portfolio[self.spy].invested:
            self.market_order(self.spy, 100)
        
        # Return if hedge position presents
        if any([self.portfolio[x.symbol].invested for x in self.tickets]):
            return

        # Return if hedge position presents
        chain = slice.option_chains.get(self.spxw)
        if not chain: return

        # Get the nearest expiry date of the contracts
        expiry = min([x.expiry for x in chain])
        
        # Select the call Option contracts with the nearest expiry and sort by strike price
        calls = sorted([i for i in chain if i.expiry == expiry and i.right == OptionRight.CALL], 
                        key=lambda x: x.strike)
        if len(calls) < 2: return

        # Buy the bull call spread
        bull_call_spread = OptionStrategies.bull_call_spread(self.spxw, calls[0].strike, calls[-1].strike, expiry)
        self.tickets = self.buy(bull_call_spread, 1)