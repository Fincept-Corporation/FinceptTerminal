# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
# 
# Licensed under the Apache License, Version 2.0 (the "License"); 
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Modified by Fincept Corporation for the Fincept Terminal: comment text changed; the code is unchanged.

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
