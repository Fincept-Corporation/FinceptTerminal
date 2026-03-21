# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5A449593
# Category: Regression Test
# Description: Regression algorithm testing GH feature 3790, using SetHoldings with a collection of targets which will be ordered by...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm testing GH feature 3790, using SetHoldings with a collection of targets
### which will be ordered by margin impact before being executed, with the objective of avoiding any
### margin errors
### </summary>
class SetHoldingsMultipleTargetsRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10,11)

        # use leverage 1 so we test the margin impact ordering
        self._spy = self.add_equity("SPY", Resolution.MINUTE, Market.USA, False, 1).symbol
        self._ibm = self.add_equity("IBM", Resolution.MINUTE, Market.USA, False, 1).symbol

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings([PortfolioTarget(self._spy, 0.8), PortfolioTarget(self._ibm, 0.2)])
        else:
            self.set_holdings([PortfolioTarget(self._ibm, 0.8), PortfolioTarget(self._spy, 0.2)])
