# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3A64A758
# Category: Corporate Actions
# Description: Demonstration of payments for cash dividends in backtesting. When data normalization mode is set to "Raw" the dividen...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of payments for cash dividends in backtesting. When data normalization mode is set
### to "Raw" the dividends are paid as cash directly into your portfolio.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="data event handlers" />
### <meta name="tag" content="dividend event" />
class DividendAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(1998,1,1)  #Set Start Date
        self.set_end_date(2006,1,21)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        equity = self.add_equity("MSFT", Resolution.DAILY)
        equity.set_data_normalization_mode(DataNormalizationMode.RAW)

        # this will use the Tradier Brokerage open order split behavior
        # forward split will modify open order to maintain order value
        # reverse split open orders will be cancelled
        self.set_brokerage_model(BrokerageName.TRADIER_BROKERAGE)


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        bar = data["MSFT"]
        if self.transactions.orders_count == 0:
            self.set_holdings("MSFT", .5)
            # place some orders that won't fill, when the split comes in they'll get modified to reflect the split
            quantity = self.calculate_order_quantity("MSFT", .25)
            self.debug(f"Purchased Stock: {bar.price}")
            self.stop_market_order("MSFT", -quantity, bar.low/2)
            self.limit_order("MSFT", -quantity, bar.high*2)

        if data.dividends.contains_key("MSFT"):
            dividend = data.dividends["MSFT"]
            self.log(f"{self.time} >> DIVIDEND >> {dividend.symbol} - {dividend.distribution} - {self.portfolio.cash} - {self.portfolio['MSFT'].price}")

        if data.splits.contains_key("MSFT"):
            split = data.splits["MSFT"]
            self.log(f"{self.time} >> SPLIT >> {split.symbol} - {split.split_factor} - {self.portfolio.cash} - {self.portfolio['MSFT'].price}")

    def on_order_event(self, order_event):
        # orders get adjusted based on split events to maintain order value
        order = self.transactions.get_order_by_id(order_event.order_id)
        self.log(f"{self.time} >> ORDER >> {order}")
