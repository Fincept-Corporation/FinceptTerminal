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
### In this algorithm we demonstrate how to use the UniverseSettings
### to define the data normalization mode (raw)
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="coarse universes" />
### <meta name="tag" content="fine universes" />
class RawPricesUniverseRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # what resolution should the data *added* to the universe be?
        self.universe_settings.resolution = Resolution.DAILY

        # Use raw prices
        self.universe_settings.data_normalization_mode = DataNormalizationMode.RAW

        self.set_start_date(2014,3,24)    #Set Start Date
        self.set_end_date(2014,4,7)      #Set End Date
        self.set_cash(50000)            #Set Strategy Cash

        # Set the security initializer with zero fees and price initial seed
        securitySeeder = FuncSecuritySeeder(self.GetLastKnownPrices)
        self.set_security_initializer(CompositeSecurityInitializer(
            FuncSecurityInitializer(lambda x: x.set_fee_model(ConstantFeeModel(0))),
            FuncSecurityInitializer(lambda security: securitySeeder.SeedSecurity(security))))

        self.add_universe("MyUniverse", Resolution.DAILY, self.selection_function)


    def selection_function(self, date_time):
        if date_time.day % 2 == 0:
            return ["SPY", "IWM", "QQQ"]
        else:
            return ["AIG", "BAC", "IBM"]


    # this event fires whenever we have changes to our universe
    def on_securities_changed(self, changes):
        # liquidate removed securities
        for security in changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        # we want 20% allocation in each security in our universe
        for security in changes.added_securities:
            self.set_holdings(security.symbol, 0.2)
