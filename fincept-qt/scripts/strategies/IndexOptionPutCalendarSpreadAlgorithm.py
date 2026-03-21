# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E0220F96
# Category: Options
# Description: Index Option Put Calendar Spread Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class IndexOptionPutCalendarSpreadAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 1, 1)
        self.set_end_date(2023, 1, 1)
        self.set_cash(50000)

        self.vxz = self.add_equity("VXZ", Resolution.MINUTE).symbol

        index = self.add_index("VIX", Resolution.MINUTE).symbol
        option = self.add_index_option(index, "VIXW", Resolution.MINUTE)
        option.set_filter(lambda x: x.strikes(-2, 2).expiration(15, 45))
        
        self.vixw = option.symbol
        self.tickets = []
        self.expiry = datetime.max

    def on_data(self, slice: Slice) -> None:
        if not self.portfolio[self.vxz].invested:
            self.market_order(self.vxz, 100)
        
        index_options_invested = [leg for leg in self.tickets if self.portfolio[leg.symbol].invested]
        # Liquidate if the shorter term option is about to expire
        if self.expiry < self.time + timedelta(2) and all([slice.contains_key(x.symbol) for x in self.tickets]):
            for holding in index_options_invested:
                self.liquidate(holding.symbol)
        # Return if there is any opening index option position
        elif index_options_invested:
            return

        # Get the OptionChain
        chain = slice.option_chains.get(self.vixw)
        if not chain: return

        # Get ATM strike price
        strike = sorted(chain, key = lambda x: abs(x.strike - chain.underlying.value))[0].strike
        
        # Select the ATM put Option contracts and sort by expiration date
        puts = sorted([i for i in chain if i.strike == strike and i.right == OptionRight.PUT], 
                        key=lambda x: x.expiry)
        if len(puts) < 2: return
        self.expiry = puts[0].expiry

        # Sell the put calendar spread
        put_calendar_spread = OptionStrategies.put_calendar_spread(self.vixw, strike, self.expiry, puts[-1].expiry)
        self.tickets = self.sell(put_calendar_spread, 1, asynchronous=True)