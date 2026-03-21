# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-BB3C695A
# Category: Universe Selection
# Description: Regression algorithm which tests a fine fundamental filtered universe, related to GH issue 4127
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm which tests a fine fundamental filtered universe,
### related to GH issue 4127
### </summary>
class FineFundamentalFilteredUniverseRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2014, 10, 8)
        self.set_end_date(2014, 10, 13)

        self.universe_settings.resolution = Resolution.DAILY

        symbol = Symbol(SecurityIdentifier.generate_constituent_identifier("constituents-universe-qctest", SecurityType.EQUITY, Market.USA), "constituents-universe-qctest")
        self.add_universe(ConstituentsUniverse(symbol, self.universe_settings), self.fine_selection_function)

    def fine_selection_function(self, fine):
        return [ x.symbol for x in fine if x.company_profile != None and x.company_profile.headquarter_city != None and x.company_profile.headquarter_city.lower() == "cupertino" ]

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            if data.keys()[0].value != "AAPL":
                raise ValueError(f"Unexpected symbol was added to the universe: {data.keys()[0]}")
            self.set_holdings(data.keys()[0], 1)
