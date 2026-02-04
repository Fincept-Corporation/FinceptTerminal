# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-13F865B8
# Category: Template
# Description: initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorith...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicTemplateIntrinioEconomicData(QCAlgorithm):

    def initialize(self):
        '''initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2010, 1, 1)  #Set Start Date
        self.set_end_date(2013, 12, 31)  #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        # Set your Intrinio user and password.
        IntrinioConfig.set_user_and_password("intrinio-username", "intrinio-password")
        # The Intrinio user and password can be also defined in the config.json file for local backtest.

        # Set Intrinio config to make 1 call each minute, default is 1 call each 5 seconds.
        #(1 call each minute is the free account limit for historical_data endpoint)
        IntrinioConfig.set_time_interval_between_calls(timedelta(minutes = 1))

        # United States Oil Fund LP
        self.uso = self.add_equity("USO", Resolution.DAILY).symbol
        self.securities[self.uso].set_leverage(2)
        # United States Brent Oil Fund LP
        self.bno = self.add_equity("BNO", Resolution.DAILY).symbol
        self.securities[self.bno].set_leverage(2)

        self.add_data(IntrinioEconomicData, "$DCOILWTICO", Resolution.DAILY)
        self.add_data(IntrinioEconomicData, "$DCOILBRENTEU", Resolution.DAILY)

        self.ema_wti = self.ema("$DCOILWTICO", 10)


    def on_data(self, slice):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.
        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if (slice.contains_key("$DCOILBRENTEU") or slice.contains_key("$DCOILWTICO")):
            spread = slice["$DCOILBRENTEU"].value - slice["$DCOILWTICO"].value
        else:
            return

        if ((spread > 0 and not self.portfolio[self.bno].is_long) or
            (spread < 0 and not self.portfolio[self.uso].is_short)):
            self.set_holdings(self.bno, 0.25 * sign(spread))
            self.set_holdings(self.uso, -0.25 * sign(spread))
