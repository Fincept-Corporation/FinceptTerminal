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
### This algorithm demonstrates how to submit orders to a Financial Advisor account group, allocation profile or a single managed account.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="financial advisor" />
class FinancialAdvisorDemoAlgorithm(QCAlgorithm):

    def initialize(self):
        # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must be initialized.

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self._symbol = self.add_equity("SPY", Resolution.SECOND).symbol

        # The default order properties can be set here to choose the FA settings
        # to be automatically used in any order submission method (such as SetHoldings, Buy, Sell and Order)

        # Use a default FA Account Group with an Allocation Method
        self.default_order_properties = InteractiveBrokersOrderProperties()
        # account group created manually in IB/TWS
        self.default_order_properties.fa_group = "TestGroupEQ"
        # supported allocation methods are: EqualQuantity, NetLiq, AvailableEquity, PctChange
        self.default_order_properties.fa_method = "EqualQuantity"

        # set a default FA Allocation Profile
        # DefaultOrderProperties = InteractiveBrokersOrderProperties()
        # allocation profile created manually in IB/TWS
        # self.default_order_properties.fa_profile = "TestProfileP"

        # send all orders to a single managed account
        # DefaultOrderProperties = InteractiveBrokersOrderProperties()
        # a sub-account linked to the Financial Advisor master account
        # self.default_order_properties.account = "DU123456"

    def on_data(self, data):
        # on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        if not self.portfolio.invested:
            # when logged into IB as a Financial Advisor, this call will use order properties
            # set in the DefaultOrderProperties property of QCAlgorithm
            self.set_holdings("SPY", 1)
