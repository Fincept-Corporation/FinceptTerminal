# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-F0179E57
# Category: General Strategy
# Description: Example algorithm of the Identity indicator with the filtering enhancement. Filtering is used to check the output of ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example algorithm of the Identity indicator with the filtering enhancement. Filtering is used to check
### the output of the indicator before returning it.
### </summary>
### <meta name="tag" content="indicators" />
### <meta name="tag" content="indicator classes" />
class FilteredIdentityAlgorithm(QCAlgorithm):
    ''' Example algorithm of the Identity indicator with the filtering enhancement '''
    
    def initialize(self):
        
        self.set_start_date(2014,5,2)       # Set Start Date
        self.set_end_date(self.start_date)   # Set End Date
        self.set_cash(100000)              # Set Stratgy Cash
 
        # Fincept Terminal Strategy Engine - Symbol Configuration
        security = self.add_forex("EURUSD", Resolution.TICK)

        self._symbol = security.symbol
        self.identity = self.filtered_identity(self._symbol, None, self.filter)
    
    def filter(self, data):
        '''Filter function: True if data is not an instance of Tick. If it is, true if TickType is Trade
        data -- Data for applying the filter'''
        if isinstance(data, Tick):
            return data.tick_type == TickType.TRADE
        return True
        
    def on_data(self, data):
        # Since we are only accepting TickType.TRADE,
        # this indicator will never be ready
        if not self.identity.is_ready: return
        if not self.portfolio.invested:
            self.set_holdings(self._symbol, 1)
            self.debug("Purchased Stock")
