# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A5E1134A
# Category: General Strategy
# Description: Demonstration of the Market On Close order for US Equities
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of the Market On Close order for US Equities.
### </summary>
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="placing orders" />
class MarketOnOpenOnCloseAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.equity = self.add_equity("SPY", Resolution.SECOND, fill_forward = True, extended_market_hours = True)
        self.__submitted_market_on_close_today = False
        self.__last = datetime.min

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if self.time.date() != self.__last.date():   # each morning submit a market on open order
            self.__submitted_market_on_close_today = False
            self.market_on_open_order("SPY", 100)
            self.__last = self.time

        if not self.__submitted_market_on_close_today and self.equity.exchange.exchange_open:   # once the exchange opens submit a market on close order
            self.__submitted_market_on_close_today = True
            self.market_on_close_order("SPY", -100)

    def on_order_event(self, fill):
        order = self.transactions.get_order_by_id(fill.order_id)
        self.log("{0} - {1}:: {2}".format(self.time, order.type, fill))
