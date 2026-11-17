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
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

from AlgorithmImports import *

### <summary>
### Basic template framework algorithm uses framework components to define the algorithm.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class IndiaDataRegressionAlgorithm(QCAlgorithm):
    '''Basic template framework algorithm uses framework components to define the algorithm.'''

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        
        self.set_account_currency("INR")
        self.set_start_date(2004, 5, 20)
        self.set_end_date(2016, 7, 26) 
        self._mapping_symbol = self.add_equity("3MINDIA", Resolution.DAILY, Market.INDIA).symbol
        self._split_and_dividend_symbol = self.add_equity("CCCL", Resolution.DAILY, Market.INDIA).symbol
        self._received_warning_event = False
        self._received_occurred_event = False
        self._initial_mapping = False
        self._execution_mapping = False
        self.debug("numpy test >>> print numpy.pi: " + str(np.pi))

    def on_dividends(self, dividends: Dividends):
        if dividends.contains_key(self._split_and_dividend_symbol):
            dividend = dividends[self._split_and_dividend_symbol]
            if ((self.time.year == 2010 and self.time.month == 6 and self.time.day == 15) and
                    (dividend.price != 0.5 or dividend.reference_price != 88.8 or dividend.distribution != 0.5)):
                raise Exception("Did not receive expected dividend values")

    def on_splits(self, splits: Splits):
        if splits.contains_key(self._split_and_dividend_symbol):
            split = splits[self._split_and_dividend_symbol]
            if split.type == SplitType.WARNING:
                self._received_warning_event = True
            elif split.type == SplitType.SPLIT_OCCURRED:
                self._received_occurred_event = True
                if split.price != 421.0 or split.reference_price != 421.0 or split.split_factor != 0.2:
                    raise Exception("Did not receive expected price values")

    def on_symbol_changed_events(self, symbols_changed: SymbolChangedEvents):
        if symbols_changed.contains_key(self._mapping_symbol):
                mapping_event = [x.value for x in symbols_changed if x.key.security_type == 1][0]
                if self.time.year == 1999 and self.time.month == 1 and self.time.day == 1:
                    self._initial_mapping = True
                elif self.time.year == 2004 and self.time.month == 6 and self.time.day == 15:
                    if mapping_event.new_symbol == "3MINDIA" and mapping_event.old_symbol == "BIRLA3M":
                        self._execution_mapping = True
    
    def on_end_of_algorithm(self):
        if self._initial_mapping:
            raise Exception("The ticker generated the initial rename event")

        if not self._execution_mapping:
            raise Exception("The ticker did not rename throughout the course of its life even though it should have")
        
        if not self._received_occurred_event:
            raise Exception("Did not receive expected split event")

        if not self._received_warning_event:
            raise Exception("Did not receive expected split warning event")
