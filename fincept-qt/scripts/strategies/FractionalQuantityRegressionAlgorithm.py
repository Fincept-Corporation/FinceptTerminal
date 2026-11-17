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
### Regression algorithm for fractional forex pair
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="regression test" />
class FractionalQuantityRegressionAlgorithm(QCAlgorithm):

    def initialize(self):

        self.set_start_date(2015, 11, 12)
        self.set_end_date(2016, 4, 1)
        self.set_cash(100000)
        self.set_brokerage_model(BrokerageName.GDAX, AccountType.CASH)

        self.set_time_zone(TimeZones.UTC)

        security = self.add_security(SecurityType.CRYPTO, "BTCUSD", Resolution.DAILY, Market.GDAX, False, 1, True)

        ### The default buying power model for the Crypto security type is now CashBuyingPowerModel.
        ### Since this test algorithm uses leverage we need to set a buying power model with margin.
        security.set_buying_power_model(SecurityMarginModel(3.3))

        con = TradeBarConsolidator(1)
        self.subscription_manager.add_consolidator("BTCUSD", con)
        con.data_consolidated += self.data_consolidated
        self.set_benchmark(security.symbol)

    def data_consolidated(self, sender, bar):
        quantity = math.floor((self.portfolio.cash + self.portfolio.total_fees) / abs(bar.value + 1))
        btc_qnty = float(self.portfolio["BTCUSD"].quantity)

        if not self.portfolio.invested:
            self.order("BTCUSD", quantity)
        elif btc_qnty == quantity:
            self.order("BTCUSD", 0.1)
        elif btc_qnty == quantity + 0.1:
            self.order("BTCUSD", 0.01)
        elif btc_qnty == quantity + 0.11:
            self.order("BTCUSD", -0.02)
        elif btc_qnty == quantity + 0.09:
            # should fail (below minimum order quantity)
            self.order("BTCUSD", 0.00001)
            self.set_holdings("BTCUSD", -2.0)
            self.set_holdings("BTCUSD", 2.0)
            self.quit()
