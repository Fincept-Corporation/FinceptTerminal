# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-CA7ACD88
# Category: Futures
# Description: EMA cross with SP500 E-mini futures In this example, we demostrate how to trade futures contracts using a equity to g...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### EMA cross with SP500 E-mini futures
### In this example, we demostrate how to trade futures contracts using
### a equity to generate the trading signals
### It also shows how you can prefilter contracts easily based on expirations.
### It also shows how you can inspect the futures chain to pick a specific contract to trade.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="futures" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="strategy example" />
class FuturesMomentumAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2016, 1, 1)
        self.set_end_date(2016, 8, 18)
        self.set_cash(100000)
        fast_period = 20
        slow_period = 60
        self._tolerance = 1 + 0.001
        self.is_up_trend = False
        self.is_down_trend = False
        self.set_warm_up(max(fast_period, slow_period))

        # Adds SPY to be used in our EMA indicators
        equity = self.add_equity("SPY", Resolution.DAILY)
        self._fast = self.ema(equity.symbol, fast_period, Resolution.DAILY)
        self._slow = self.ema(equity.symbol, slow_period, Resolution.DAILY)
        # Adds the future that will be traded and
        # set our expiry filter for this futures chain
        future = self.add_future(Futures.Indices.SP_500_E_MINI)
        future.set_filter(timedelta(0), timedelta(182))

    def on_data(self, slice):
        if self._slow.is_ready and self._fast.is_ready:
            self.is_up_trend = self._fast.current.value > self._slow.current.value * self._tolerance
            self.is_down_trend = self._fast.current.value < self._slow.current.value * self._tolerance
            if (not self.portfolio.invested) and self.is_up_trend:
                for chain in slice.futures_chains:
                    # find the front contract expiring no earlier than in 90 days
                    contracts = list(filter(lambda x: x.expiry > self.time + timedelta(90), chain.value))
                    # if there is any contract, trade the front contract
                    if len(contracts) == 0: continue
                    contract = sorted(contracts, key = lambda x: x.expiry, reverse=True)[0]
                    self.market_order(contract.symbol , 1)

            if self.portfolio.invested and self.is_down_trend:
                self.liquidate()

    def on_end_of_day(self, symbol):
        if self.is_up_trend:
            self.plot("Indicator Signal", "EOD",1)
        elif self.is_down_trend:
            self.plot("Indicator Signal", "EOD",-1)
        elif self._slow.is_ready and self._fast.is_ready:
            self.plot("Indicator Signal", "EOD",0)

    def on_order_event(self, order_event):
        self.log(str(order_event))
