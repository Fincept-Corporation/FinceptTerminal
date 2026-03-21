# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-DE244AD5
# Category: Universe Selection
# Description: Test algorithm using a 'ConstituentsUniverse' with test data
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Test algorithm using a 'ConstituentsUniverse' with test data
### </summary>
class ConstituentsUniverseRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)  #Set Start Date
        self.set_end_date(2013, 10, 11)    #Set End Date
        self.set_cash(100000)             #Set Strategy Cash

        self._appl = Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)
        self._spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        self._qqq = Symbol.create("QQQ", SecurityType.EQUITY, Market.USA)
        self._fb = Symbol.create("FB", SecurityType.EQUITY, Market.USA)
        self._step = 0

        self.universe_settings.resolution = Resolution.DAILY

        custom_universe_symbol = Symbol(SecurityIdentifier.generate_constituent_identifier(
                    "constituents-universe-qctest",
                    SecurityType.EQUITY,
                    Market.USA),
                "constituents-universe-qctest")

        self.add_universe(ConstituentsUniverse(custom_universe_symbol, self.universe_settings))

    def on_data(self, data):
        self._step = self._step + 1
        if self._step == 1:
            if not data.contains_key(self._qqq) or not data.contains_key(self._appl):
                raise ValueError("Unexpected symbols found, step: " + str(self._step))
            if data.count != 2:
                raise ValueError("Unexpected data count, step: " + str(self._step))
            # AAPL will be deselected by the ConstituentsUniverse
            # but it shouldn't be removed since we hold it
            self.set_holdings(self._appl, 0.5)
        elif self._step == 2:
            if not data.contains_key(self._appl):
                raise ValueError("Unexpected symbols found, step: " + str(self._step))
            if data.count != 1:
                raise ValueError("Unexpected data count, step: " + str(self._step))
            # AAPL should now be released
            # note: takes one extra loop because the order is executed on market open
            self.liquidate()
        elif self._step == 3:
            if not data.contains_key(self._fb) or not data.contains_key(self._spy) or not data.contains_key(self._appl):
                raise ValueError("Unexpected symbols found, step: " + str(self._step))
            if data.count != 3:
                raise ValueError("Unexpected data count, step: " + str(self._step))
        elif self._step == 4:
            if not data.contains_key(self._fb) or not data.contains_key(self._spy):
                raise ValueError("Unexpected symbols found, step: " + str(self._step))
            if data.count != 2:
                raise ValueError("Unexpected data count, step: " + str(self._step))

    def on_end_of_algorithm(self):
        # First selection is on the midnight of the 8th, start getting data from the 8th to the 11th
        if self._step != 4:
            raise ValueError("Unexpected step count: " + str(self._step))

    def  OnSecuritiesChanged(self, changes):
        for added in changes.added_securities:
            self.log(f"{self.Time} AddedSecurities " + str(added))

        for removed in changes.removed_securities:
            self.log(f"{self.Time} RemovedSecurities " + str(removed) + str(self._step))
            # we are currently notifying the removal of AAPl twice,
            # when deselected and when finally removed (since it stayed pending)
            if removed.symbol == self._appl and self._step != 1 and self._step != 2 or removed.symbol == self._qqq and self._step != 1:
                raise ValueError("Unexpected removal step count: " + str(self._step))
