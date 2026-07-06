# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-94DBB773
# Category: Regression Test
# Description: Demonstration of requesting daily resolution data for US Equities. This is a simple regression test algorithm using a...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of requesting daily resolution data for US Equities.
### This is a simple regression test algorithm using a skeleton algorithm and requesting daily data.
### </summary>
### <meta name="tag" content="using data" />
class NamedArgumentsRegression(QCAlgorithm):
    '''Regression algorithm that makes use of PythonNet kwargs'''

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        #Use named args for setting up our algorithm
        self.set_start_date(month=10,day=8,year=2013)   #Set Start Date
        self.set_end_date(month=10,day=17,year=2013)    #Set End Date
        self.set_cash(starting_cash=100000)           #Set Strategy Cash

        #Check our values
        if self.start_date.year != 2013 or self.start_date.month != 10 or self.start_date.day != 8:
            raise AssertionError(f"Start date was incorrect! Expected 10/8/2013 Recieved {self.start_date}")

        if self.end_date.year != 2013 or self.end_date.month != 10 or self.end_date.day != 17:
            raise AssertionError(f"End date was incorrect! Expected 10/17/2013 Recieved {self.end_date}")

        if self.portfolio.cash != 100000:
            raise AssertionError(f"Portfolio cash was incorrect! Expected 100000 Recieved {self.portfolio.cash}")

        # Use named args for addition of this security to our algorithm
        symbol = self.add_equity(resolution=Resolution.DAILY, ticker="SPY").symbol

        # Check our subscriptions for the symbol and check its resolution
        for config in self.subscription_manager.subscription_data_config_service.get_subscription_data_configs(symbol):
            if config.resolution != Resolution.DAILY:
                raise AssertionError(f"Resolution was not correct on security")


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings(symbol="SPY", percentage=1)
            self.debug(message="Purchased Stock")
