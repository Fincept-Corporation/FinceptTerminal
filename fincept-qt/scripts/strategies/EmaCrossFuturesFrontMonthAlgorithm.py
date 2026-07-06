# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-EE365736
# Category: Futures
# Description: This example demonstrates how to implement a cross moving average for the futures front contract
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This example demonstrates how to implement a cross moving average for the futures front contract
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="indicator" />
### <meta name="tag" content="futures" />
class EmaCrossFuturesFrontMonthAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 8)
        self.set_end_date(2013, 10, 10)
        self.set_cash(1000000)

        future = self.add_future(Futures.Metals.GOLD)

        # Only consider the front month contract
        # Update the universe once per day to improve performance
        future.set_filter(lambda x: x.front_month().only_apply_filter_at_market_open())

        # Symbol of the current contract
        self._symbol = None

        # Create two exponential moving averages
        self.fast = ExponentialMovingAverage(100)
        self.slow = ExponentialMovingAverage(300)
        self.tolerance = 0.001
        self.consolidator = None

        # Add a custom chart to track the EMA cross
        chart = Chart('EMA Cross')
        chart.add_series(Series('Fast', SeriesType.LINE, 0))
        chart.add_series(Series('Slow', SeriesType.LINE, 0))
        self.add_chart(chart)

    def on_data(self,slice):

        holding = None if self._symbol is None else self.portfolio.get(self._symbol)
        if holding is not None:
            # Buy the futures' front contract when the fast EMA is above the slow one
            if self.fast.current.value > self.slow.current.value * (1 + self.tolerance):
                if not holding.invested:
                    self.set_holdings(self._symbol, .1)
                    self.plot_ema()
            elif holding.invested:
                self.liquidate(self._symbol)
                self.plot_ema()

    def on_securities_changed(self, changes):
        if len(changes.removed_securities) > 0:
            # Remove the consolidator for the previous contract
            # and reset the indicators
            if self._symbol is not None and self.consolidator is not None:
                self.subscription_manager.remove_consolidator(self._symbol, self.consolidator)
                self.fast.reset()
                self.slow.reset()
            # We don't need to call Liquidate(_symbol),
            # since its positions are liquidated because the contract has expired.

        # Only one security will be added: the new front contract
        self._symbol = changes.added_securities[0].symbol

        # Create a new consolidator and register the indicators to it
        self.consolidator = self.resolve_consolidator(self._symbol, Resolution.MINUTE)
        self.register_indicator(self._symbol, self.fast, self.consolidator)
        self.register_indicator(self._symbol, self.slow, self.consolidator)

        #  Warm up the indicators
        self.warm_up_indicator(self._symbol, self.fast, Resolution.MINUTE)
        self.warm_up_indicator(self._symbol, self.slow, Resolution.MINUTE)

        self.plot_ema()

    def plot_ema(self):
        self.plot('EMA Cross', 'Fast', self.fast.current.value)
        self.plot('EMA Cross', 'Slow', self.slow.current.value)
