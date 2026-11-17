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
### Example algorithm using and asserting the behavior of auxiliary data handlers
### </summary>
class AuxiliaryDataHandlersRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2007, 5, 16)
        self.set_end_date(2015, 1, 1)

        self.universe_settings.resolution = Resolution.DAILY

        # will get delisted
        self.add_equity("AAA.1")

        # get's remapped
        self.add_equity("SPWR")

        # has a split & dividends
        self.add_equity("AAPL")

    def on_delistings(self, delistings: Delistings):
        self._on_delistings_called = True

    def on_symbol_changed_events(self, symbolsChanged: SymbolChangedEvents):
        self._on_symbol_changed_events = True

    def on_splits(self, splits: Splits):
        self._on_splits = True

    def on_dividends(self, dividends: Dividends):
        self._on_dividends = True

    def on_end_of_algorithm(self):
        if not self._on_delistings_called:
            raise ValueError("OnDelistings was not called!")
        if not self._on_symbol_changed_events:
            raise ValueError("OnSymbolChangedEvents was not called!")
        if not self._on_splits:
            raise ValueError("OnSplits was not called!")
        if not self._on_dividends:
            raise ValueError("OnDividends was not called!")
