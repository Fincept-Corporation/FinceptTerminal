# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-07CF5BDC
# Category: Template
# Description: The demonstration algorithm shows some of the most common order methods when working with CFD assets
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### The demonstration algorithm shows some of the most common order methods when working with CFD assets.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />

class BasicTemplateCfdAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_account_currency('EUR')

        self.set_start_date(2019, 2, 20)
        self.set_end_date(2019, 2, 21)
        self.set_cash('EUR', 100000)

        self._symbol = self.add_cfd('DE30EUR').symbol

        # Historical Data
        history = self.history(self._symbol, 60, Resolution.DAILY)
        self.log(f"Received {len(history)} bars from CFD historical data call.")

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.
        Arguments:
            slice: Slice object keyed by symbol containing the stock data
        '''
        # Access Data
        if data.quote_bars.contains_key(self._symbol):
            quote_bar = data.quote_bars[self._symbol]
            self.log(f"{quote_bar.end_time} :: {quote_bar.close}")

        if not self.portfolio.invested:
            self.set_holdings(self._symbol, 1)

    def on_order_event(self, order_event):
        self.debug("{} {}".format(self.time, order_event.to_string()))
