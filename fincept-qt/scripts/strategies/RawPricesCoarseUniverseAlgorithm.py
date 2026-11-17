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
### In this algorithm we demonstrate how to use the coarse fundamental data to define a universe as the top dollar volume and set the algorithm to use raw prices
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
### <meta name="tag" content="fine universes" />
class RawPricesCoarseUniverseAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # what resolution should the data *added* to the universe be?
        self.universe_settings.resolution = Resolution.DAILY

        self.set_start_date(2014,1,1)    #Set Start Date
        self.set_end_date(2015,1,1)      #Set End Date
        self.set_cash(50000)            #Set Strategy Cash

        # Set the security initializer with the characteristics defined in CustomSecurityInitializer
        self.set_security_initializer(self.custom_security_initializer)

        # this add universe method accepts a single parameter that is a function that
        # accepts an IEnumerable<CoarseFundamental> and returns IEnumerable<Symbol>
        self.add_universe(self.coarse_selection_function)

        self.__number_of_symbols = 5

    def custom_security_initializer(self, security):
        '''Initialize the security with raw prices and zero fees 
        Args:
            security: Security which characteristics we want to change'''
        security.set_data_normalization_mode(DataNormalizationMode.RAW)
        security.set_fee_model(ConstantFeeModel(0))

    # sort the data by daily dollar volume and take the top 'NumberOfSymbols'
    def coarse_selection_function(self, coarse):
        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted(coarse, key=lambda x: x.dollar_volume, reverse=True)

        # return the symbol objects of the top entries from our sorted collection
        return [ x.symbol for x in sorted_by_dollar_volume[:self.__number_of_symbols] ]


    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        # liquidate removed securities
        for security in changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        # we want 20% allocation in each security in our universe
        for security in changes.added_securities:
            self.set_holdings(security.symbol, 0.2)

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.log(f"OnOrderEvent({self.utc_time}):: {order_event}")
