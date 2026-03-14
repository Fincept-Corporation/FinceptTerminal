# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-77306E7F
# Category: Index
# Description: Basic Template India Index Algorithm uses framework components to define the algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Basic Template India Index Algorithm uses framework components to define the algorithm.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class BasicTemplateIndiaIndexAlgorithm(QCAlgorithm):
    '''Basic template framework algorithm uses framework components to define the algorithm.'''

    def initialize(self):
        '''initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_account_currency("INR") #Set Account Currency
        self.set_start_date(2019, 1, 1)  #Set Start Date
        self.set_end_date(2019, 1, 5)    #Set End Date
        self.set_cash(1000000)          #Set Strategy Cash

        # Use indicator for signal; but it cannot be traded
        self.nifty = self.add_index("NIFTY50", Resolution.MINUTE, Market.INDIA).symbol
        # Trade Index based ETF
        self.nifty_etf = self.add_equity("JUNIORBEES", Resolution.MINUTE, Market.INDIA).symbol
   
        # Set Order Properties as per the requirements for order placement
        self.default_order_properties = IndiaOrderProperties(Exchange.NSE)

        # Define indicator
        self._ema_slow = self.ema(self.nifty, 80)
        self._ema_fast = self.ema(self.nifty, 200)

        self.debug("numpy test >>> print numpy.pi: " + str(np.pi))


    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''

        if not data.bars.contains_key(self.nifty) or not data.bars.contains_key(self.nifty_etf):
            return

        if not self._ema_slow.is_ready:
            return

        if self._ema_fast > self._ema_slow:
            if not self.portfolio.invested:
                self.market_ticket = self.market_order(self.nifty_etf, 1)
        else:
            self.liquidate()


    def on_end_of_algorithm(self):
        if self.portfolio[self.nifty].total_sale_volume > 0:
            raise Exception("Index is not tradable.")

