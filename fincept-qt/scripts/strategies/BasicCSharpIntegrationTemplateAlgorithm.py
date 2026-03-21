# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3B2A7C04
# Category: Template
# Description: OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicCSharpIntegrationTemplateAlgorithm(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2013,10, 7)  #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        
        self.add_equity("SPY", Resolution.SECOND)
        
    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)

            ## Calculate value of sin(10) for both python and C#
            self.debug(f'According to Python, the value of sin(10) is {np.sin(10)}')
            self.debug(f'According to C#, the value of sin(10) is {Math.sin(10)}')
