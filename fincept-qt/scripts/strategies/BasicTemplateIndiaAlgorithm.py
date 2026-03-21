# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-CB56992B
# Category: Template
# Description: Basic template framework algorithm uses framework components to define the algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Basic template framework algorithm uses framework components to define the algorithm.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class BasicTemplateIndiaAlgorithm(QCAlgorithm):
    '''Basic template framework algorithm uses framework components to define the algorithm.'''

    def initialize(self):
        '''initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_account_currency("INR")  #Set Account Currency 
        self.set_start_date(2019, 1, 23)  #Set Start Date
        self.set_end_date(2019, 10, 31)   #Set End Date
        self.set_cash(100000)            #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("YESBANK", Resolution.MINUTE, Market.INDIA)
        self.debug("numpy test >>> print numpy.pi: " + str(np.pi))

        # Set Order Properties as per the requirements for order placement
        self.default_order_properties = IndiaOrderProperties(Exchange.NSE)

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.market_order("YESBANK", 1)

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug("Purchased Stock: {0}".format(order_event.symbol))
