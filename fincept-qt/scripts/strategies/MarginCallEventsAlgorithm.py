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
### This algorithm showcases two margin related event handlers.
### OnMarginCallWarning: Fired when a portfolio's remaining margin dips below 5% of the total portfolio value
### OnMarginCall: Fired immediately before margin call orders are execued, this gives the algorithm a change to regain margin on its own through liquidation
### </summary>
### <meta name="tag" content="securities and portfolio" />
### <meta name="tag" content="margin models" />
class MarginCallEventsAlgorithm(QCAlgorithm):
    """
    This algorithm showcases two margin related event handlers.
    on_margin_call_warning: Fired when a portfolio's remaining margin dips below 5% of the total portfolio value
    on_margin_call: Fired immediately before margin call orders are execued, this gives the algorithm a change to regain margin on its own through liquidation
    """

    def initialize(self):
        self.set_cash(100000)
        self.set_start_date(2013,10,1)
        self.set_end_date(2013,12,11)
        self.add_equity("SPY", Resolution.SECOND)
        # cranking up the leverage increases the odds of a margin call
        # when the security falls in value
        self.securities["SPY"].set_leverage(100)

    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings("SPY",100)

    def on_margin_call(self, requests):
        # Margin call event handler. This method is called right before the margin call orders are placed in the market.
        # <param name="requests">The orders to be executed to bring this algorithm within margin limits</param>
        # this code gets called BEFORE the orders are placed, so we can try to liquidate some of our positions
        # before we get the margin call orders executed. We could also modify these orders by changing their quantities
        for order in requests:

            # liquidate an extra 10% each time we get a margin call to give us more padding
            new_quantity = int(order.quantity * 1.1)
            requests.remove(order)
            requests.append(SubmitOrderRequest(order.order_type, order.security_type, order.symbol, new_quantity, order.stop_price, order.limit_price, self.time, "on_margin_call"))

        return requests

    def on_margin_call_warning(self):
        # Margin call warning event handler.
        # This method is called when portfolio.margin_remaining is under 5% of your portfolio.total_portfolio_value
        # a chance to prevent a margin call from occurring

        spy_holdings = self.securities["SPY"].holdings.quantity
        shares = int(-spy_holdings * 0.005)
        self.error("{0} - on_margin_call_warning(): Liquidating {1} shares of SPY to avoid margin call.".format(self.time, shares))
        self.market_order("SPY", shares)
