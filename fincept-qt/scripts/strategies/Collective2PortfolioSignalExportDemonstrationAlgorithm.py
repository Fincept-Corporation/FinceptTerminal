# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5600B07D
# Category: Portfolio Management
# Description: This algorithm sends a list of portfolio targets from algorithm's Portfolio to Collective2 API every time the ema ind...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This algorithm sends a list of portfolio targets from algorithm's Portfolio
### to Collective2 API every time the ema indicators crosses between themselves.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="securities and portfolio" />
class Collective2PortfolioSignalExportDemonstrationAlgorithm(QCAlgorithm):

    def initialize(self):
        ''' Initialize the date and add all equity symbols present in list _symbols '''

        self.set_start_date(2013, 10, 7)   #Set Start Date
        self.set_end_date(2013, 10, 11)    #Set End Date
        self.set_cash(100000)             #Set Strategy Cash

        # Symbols accepted by Collective2. Collective2 accepts stock, future, forex and US stock option symbols
        self.add_equity("GOOG")
        self._symbols = [
            Symbol.create("SPY", SecurityType.EQUITY, Market.USA, None, None),
            Symbol.create("EURUSD", SecurityType.FOREX, Market.OANDA, None, None),
            Symbol.create_future("ES", Market.CME, datetime(2023, 12, 15), None),
            Symbol.create_option("GOOG", Market.USA, OptionStyle.AMERICAN, OptionRight.CALL, 130, datetime(2023, 9, 1))
        ]
        for item in self._symbols:
            self.add_security(item)

        self.fast = self.ema("SPY", 10)
        self.slow = self.ema("SPY", 100)

        # Initialize these flags, to check when the ema indicators crosses between themselves
        self.ema_fast_is_not_set = True
        self.ema_fast_was_above = False

        # Collective2 APIv4 KEY: This value is provided by Collective2 in their webpage in your account section (See https://collective2.com/account-info)
        # See API documentation at https://trade.collective2.com/c2-api
        self.collective2_apikey = "YOUR APIV4 KEY"

        # Collective2 System ID: This value is found beside the system's name (strategy's name) on the main system page
        self.collective2_system_id = 0

        self.signal_export.add_signal_export_providers(Collective2SignalExport(self.collective2_apikey, self.collective2_system_id))

        self.first_call = True

        self.set_warm_up(100)

    def on_data(self, data):
        ''' Reduce the quantity of holdings for one security and increase the holdings to the another
        one when the EMA's indicators crosses between themselves, then send a signal to Collective2 API '''
        if self.is_warming_up: return

        # Place an order as soon as possible to send a signal.
        if self.first_call:
            self.set_holdings("SPY", 0.1)
            self.signal_export.set_target_portfolio_from_portfolio()
            self.first_call = False

        fast = self.fast.current.value
        slow = self.slow.current.value

        # Set the value of flag _ema_fast_was_above, to know when the ema indicators crosses between themselves
        if self.ema_fast_is_not_set == True:
            if fast > slow *1.001:
                self.ema_fast_was_above = True
            else:
                self.ema_fast_was_above = False
            self.ema_fast_is_not_set = False

        # Check whether ema fast and ema slow crosses. If they do, set holdings to SPY
        # or reduce its holdings, and send signals to Collective2 API from your Portfolio
        if fast > slow * 1.001 and (not self.ema_fast_was_above):
            self.set_holdings("SPY", 0.1)
            self.signal_export.set_target_portfolio_from_portfolio()
        elif fast < slow * 0.999 and (self.ema_fast_was_above):
            self.set_holdings("SPY", 0.01)
            self.signal_export.set_target_portfolio_from_portfolio()
