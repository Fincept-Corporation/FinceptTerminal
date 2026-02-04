# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D4F95392
# Category: Data Consolidation
# Description: This regression algorithm asserts the consolidated US equity daily bars from the hour bars exactly matches the daily ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm asserts the consolidated US equity daily bars from the hour bars exactly matches
### the daily bars returned from the database
### </summary>
class ConsolidateHourBarsIntoDailyBarsRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2020, 5, 1)
        self.set_end_date(2020, 6, 5)

        self.spy = self.add_equity("SPY", Resolution.HOUR).symbol

        # We will use these two indicators to compare the daily consolidated bars equals
        # the ones returned from the database. We use this specific type of indicator as
        # it depends on its previous values. Thus, if at some point the bars received by
        # the indicators differ, so will their final values
        self._rsi = RelativeStrengthIndex("First", 15, MovingAverageType.WILDERS)
        self.register_indicator(self.spy, self._rsi, Resolution.DAILY, selector= lambda bar: (bar.close + bar.open) / 2)

        # We won't register this indicator as we will update it manually at the end of the
        # month, so that we can compare the values of the indicator that received consolidated
        # bars and the values of this one
        self._rsi_timedelta = RelativeStrengthIndex("Second", 15, MovingAverageType.WILDERS)
        self._values = {}
        self.count = 0;
        self._indicators_compared = False;

    def on_data(self, data: Slice):
        if self.is_warming_up:
            return

        if data.contains_key(self.spy) and data[self.spy] != None:
            if self.time.month == self.end_date.month:
                history = self.history[TradeBar](self.spy, self.count, Resolution.DAILY)
                for bar in history:
                    time = bar.end_time.strftime('%Y-%m-%d')
                    average = (bar.close + bar.open) / 2
                    self._rsi_timedelta.update(bar.end_time, average)
                    if self._rsi_timedelta.current.value != self._values[time]:
                        raise Exception(f"Both {self._rsi.name} and {self._rsi_timedelta.name} should have the same values, but they differ. {self._rsi.name}: {self._values[time]} | {self._rsi_timedelta.name}: {self._rsi_timedelta.current.value}")
                self._indicators_compared = True
                self.quit()
            else:
                time = self.time.strftime('%Y-%m-%d')
                self._values[time] = self._rsi.current.value

                # Since the symbol resolution is hour and the symbol is equity, we know the last bar received in a day will
                # be at the market close, this is 16h. We need to count how many daily bars were consolidated in order to know
                # how many we need to request from the history
                if self.time.hour == 16:
                    self.count += 1

    def on_end_of_algorithm(self):
        if not self._indicators_compared:
            raise Exception(f"Indicators {self._rsi.name} and {self._rsi_timedelta.name} should have been compared, but they were not. Please make sure the indicators are getting SPY data")
