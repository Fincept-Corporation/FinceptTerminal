# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-BFEF2627
# Category: Options
# Description: Index Option Put Butterfly Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
#endregion

class IndexOptionPutButterflyAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 1, 1)
        self.set_end_date(2021, 1, 1)
        self.set_cash(1000000)

        self.vxz = self.add_equity("VXZ", Resolution.MINUTE).symbol

        index = self.add_index("SPX", Resolution.MINUTE).symbol
        option = self.add_index_option(index, "SPXW", Resolution.MINUTE)
        option.set_filter(lambda x: x.include_weeklys().strikes(-3, 3).expiration(15, 45))

        self.spxw = option.symbol
        self.multiplier = option.symbol_properties.contract_multiplier
        self.tickets = []

    def on_data(self, slice: Slice) -> None:
        # The order of magnitude per SPXW order's value is 10000 times of VXZ
        if not self.portfolio[self.vxz].invested:
            self.market_order(self.vxz, 10000)
        
        # Return if any opening index option position
        if any([self.portfolio[x.symbol].invested for x in self.tickets]): return

        # Get the OptionChain
        chain = slice.option_chains.get(self.spxw)
        if not chain: return

        # Get nearest expiry date
        expiry = min([x.expiry for x in chain])
        
        # Select the put Option contracts with nearest expiry and sort by strike price
        puts = [x for x in chain if x.expiry == expiry and x.right == OptionRight.PUT]
        if len(puts) < 3: return
        sorted_put_strikes = sorted([x.strike for x in puts])

        # Select ATM put
        atm_strike = min([abs(x - chain.underlying.value) for x in sorted_put_strikes])

        # Get the strike prices for the ITM & OTM contracts, make sure they're in equidistance
        spread = min(atm_strike - sorted_put_strikes[0], sorted_put_strikes[-1] - atm_strike)
        otm_strike = atm_strike - spread
        itm_strike = atm_strike + spread
        if otm_strike not in sorted_put_strikes or itm_strike not in sorted_put_strikes: return
        
        # Buy the put butterfly
        put_butterfly = OptionStrategies.put_butterfly(self.spxw, itm_strike, atm_strike, otm_strike, expiry)
        price = sum([abs(self.securities[x.symbol].price * x.quantity) * self.multiplier for x in put_butterfly.underlying_legs])
        if price > 0:
            quantity = self.portfolio.total_portfolio_value // price
            self.tickets = self.buy(put_butterfly, quantity, asynchronous=True)
        