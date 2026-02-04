# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-2426C5B7
# Category: Options
# Description: The demonstration algorithm shows some of the most common order methods when working with FutureOption assets
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### The demonstration algorithm shows some of the most common order methods when working with FutureOption assets.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />

class BasicTemplateFutureOptionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2022, 1, 1)
        self.set_end_date(2022, 2, 1)
        self.set_cash(100000)

        gold_futures = self.add_future(Futures.Metals.GOLD, Resolution.MINUTE)
        gold_futures.set_filter(0, 180)
        self._symbol = gold_futures.symbol
        self.add_future_option(self._symbol, lambda universe: universe.strikes(-5, +5)
                                                                    .calls_only()
                                                                    .back_month()
                                                                    .only_apply_filter_at_market_open())

        # Historical Data
        history = self.history(self._symbol, 60, Resolution.DAILY)
        self.log(f"Received {len(history)} bars from {self._symbol} FutureOption historical data call.")

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.
        Arguments:
            slice: Slice object keyed by symbol containing the stock data
        '''
        # Access Data
        for kvp in data.option_chains:
            underlying_future_contract = kvp.key.underlying
            chain = kvp.value

            if not chain: continue

            for contract in chain:
                self.log(f"""Canonical Symbol: {kvp.key}; 
                    Contract: {contract}; 
                    Right: {contract.right}; 
                    Expiry: {contract.expiry}; 
                    Bid price: {contract.bid_price}; 
                    Ask price: {contract.ask_price}; 
                    Implied Volatility: {contract.implied_volatility}""")

            if not self.portfolio.invested:
                atm_strike = sorted(chain, key = lambda x: abs(chain.underlying.price - x.strike))[0].strike
                selected_contract = sorted([contract for contract in chain if contract.strike == atm_strike], \
                           key = lambda x: x.expiry, reverse=True)[0]
                self.market_order(selected_contract.symbol, 1)

    def on_order_event(self, order_event):
        self.debug("{} {}".format(self.time, order_event.to_string()))
