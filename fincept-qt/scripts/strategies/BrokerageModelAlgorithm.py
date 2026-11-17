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
### Demonstrate the usage of the BrokerageModel property to help improve backtesting
### accuracy through simulation of a specific brokerage's rules around restrictions
### on submitting orders as well as fee structure.
### </summary>
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="brokerage models" />
class BrokerageModelAlgorithm(QCAlgorithm):
    def initialize(self):

        self.set_cash(100000)            # Set Strategy Cash
        self.set_start_date(2013,10,7)    # Set Start Date
        self.set_end_date(2013,10,11)     # Set End Date
        self.add_equity("SPY", Resolution.SECOND)
        
        # there's two ways to set your brokerage model. The easiest would be to call
        # self.set_brokerage_model( BrokerageName ) # BrokerageName is an enum
        # self.set_brokerage_model(BrokerageName.INTERACTIVE_BROKERS_BROKERAGE)
        # self.set_brokerage_model(BrokerageName.DEFAULT)

        # the other way is to call SetBrokerageModel( IBrokerageModel ) with your
        # own custom model. I've defined a simple extension to the default brokerage
        # model to take into account a requirement to maintain 500 cash in the account at all times
        self.set_brokerage_model(MinimumAccountBalanceBrokerageModel(self,500.00))
        self.last = 1   
        
    def on_data(self, slice):
        # Simple buy and hold template
        if not self.portfolio.invested:
            self.set_holdings("SPY", self.last)
            if self.portfolio["SPY"].quantity == 0:
                # each time we fail to purchase we'll decrease our set holdings percentage
                self.debug(str(self.time) + " - Failed to purchase stock")
                self.last *= 0.95
            else:
                self.debug("{} - Purchased Stock @ SetHoldings( {} )".format(self.time, self.last))


class MinimumAccountBalanceBrokerageModel(DefaultBrokerageModel):
    '''Custom brokerage model that requires clients to maintain a minimum cash balance'''
    def __init__(self, algorithm, minimum_account_balance):
        self.algorithm = algorithm
        self.minimum_account_balance = minimum_account_balance
    
    def can_submit_order(self,security, order, message):
        '''Prevent orders which would bring the account below a minimum cash balance'''
        message = None
        # we want to model brokerage requirement of minimum_account_balance cash value in account
        order_cost = order.get_value(security)
        cash = self.algorithm.portfolio.cash
        cash_after_order = cash - order_cost
        if cash_after_order < self.minimum_account_balance:
            # return a message describing why we're not allowing this order
            message = BrokerageMessageEvent(BrokerageMessageType.WARNING, "InsufficientRemainingCapital", "Account must maintain a minimum of ${0} USD at all times. Order ID: {1}".format(self.minimum_account_balance, order.id))
            self.algorithm.error(str(message))
            return False
        return True
