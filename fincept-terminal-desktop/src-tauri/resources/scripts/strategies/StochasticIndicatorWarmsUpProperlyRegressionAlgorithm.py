# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1A300907
# Category: Indicators
# Description: Regression algorithm that asserts Stochastic indicator, registered with a different resolution consolidator, is warme...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from datetime import timedelta
from AlgorithmImports import *

### <summary>
### Regression algorithm that asserts Stochastic indicator, registered with a different resolution consolidator,
### is warmed up properly by calling QCAlgorithm.WarmUpIndicator
### </summary>
class StochasticIndicatorWarmsUpProperlyRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2020, 1, 1)  # monday = holiday..
        self.set_end_date(2020, 2, 1)
        self.set_cash(100000)

        self.data_points_received = False;
        self.spy = self.add_equity("SPY", Resolution.HOUR).symbol

        self.daily_consolidator = TradeBarConsolidator(timedelta(days=1))

        self._rsi = RelativeStrengthIndex(14, MovingAverageType.WILDERS)
        self._sto = Stochastic("FIRST", 14, 3, 3)
        self.register_indicator(self.spy, self._rsi, self.daily_consolidator)
        self.register_indicator(self.spy, self._sto, self.daily_consolidator)

        # warm_up indicator
        self.warm_up_indicator(self.spy, self._rsi, timedelta(days=1))
        self.warm_up_indicator(self.spy, self._sto, timedelta(days=1))
        

        self._rsi_history = RelativeStrengthIndex(14, MovingAverageType.WILDERS)
        self._sto_history = Stochastic("SECOND", 14, 3, 3)
        self.register_indicator(self.spy, self._rsi_history, self.daily_consolidator)
        self.register_indicator(self.spy, self._sto_history, self.daily_consolidator)

        # history warm up
        history = self.history[TradeBar](self.spy, max(self._rsi_history.warm_up_period, self._sto_history.warm_up_period), Resolution.DAILY)
        for bar in history:
            self._rsi_history.update(bar.end_time, bar.close)
            if self._rsi_history.samples == 1:
                continue
            self._sto_history.update(bar)

        indicators = [self._rsi, self._sto, self._rsi_history, self._sto_history]
        for indicator in indicators:
            if not indicator.is_ready:
                raise Exception(f"{indicator.name} should be ready, but it is not. Number of samples: {indicator.samples}")

    def on_data(self, data: Slice):
        if self.is_warming_up:
            return

        if data.contains_key(self.spy):
            self.data_points_received = True
            if self._rsi.current.value != self._rsi_history.current.value:
                raise Exception(f"Values of indicators differ: {self._rsi.name}: {self._rsi.current.value} | {self._rsi_history.name}: {self._rsi_history.current.value}")
            
            if self._sto.stoch_k.current.value != self._sto_history.stoch_k.current.value:
                raise Exception(f"Stoch K values of indicators differ: {self._sto.name}.StochK: {self._sto.stoch_k.current.value} | {self._sto_history.name}.StochK: {self._sto_history.stoch_k.current.value}")
            
            if self._sto.stoch_d.current.value != self._sto_history.stoch_d.current.value:
                raise Exception(f"Stoch D values of indicators differ: {self._sto.name}.StochD: {self._sto.stoch_d.current.value} | {self._sto_history.name}.StochD: {self._sto_history.stoch_d.current.value}")

    def on_end_of_algorithm(self):
        if not self.data_points_received:
            raise Exception("No data points received")
