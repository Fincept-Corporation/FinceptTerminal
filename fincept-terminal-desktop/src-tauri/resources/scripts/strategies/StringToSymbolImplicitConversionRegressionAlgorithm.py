# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-8FAFCC5D
# Category: Regression Test
# Description: Related to GH issue 4275, reproduces a failed string to symbol implicit conversion asserting the exception thrown con...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Related to GH issue 4275, reproduces a failed string to symbol implicit conversion asserting the exception
### thrown contains the used ticker
### </summary>
class StringToSymbolImplicitConversionRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10, 8)

        self.add_equity("SPY", Resolution.MINUTE)

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        try:
            self.market_order("PEPE", 1)
        except Exception as exception:
            if "This asset symbol (PEPE 0) was not found in your security list" in str(exception) and not self.portfolio.invested:
                self.set_holdings("SPY", 1)
