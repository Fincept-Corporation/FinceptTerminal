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
### This example demonstrates how to add futures for a given underlying asset.
### It also shows how you can prefilter contracts easily based on expirations, and how you
### can inspect the futures chain to pick a specific contract to trade.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="benchmarks" />
### <meta name="tag" content="futures" />
class BasicTemplateFuturesWithExtendedMarketAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 8)
        self.set_end_date(2013, 10, 10)
        self.set_cash(1000000)

        self.contract_symbol = None

        # Subscribe and set our expiry filter for the futures chain
        self.future_sp500 = self.add_future(Futures.Indices.SP_500_E_MINI, extended_market_hours = True)
        self.future_gold = self.add_future(Futures.Metals.GOLD, extended_market_hours = True)

        # set our expiry filter for this futures chain
        # SetFilter method accepts timedelta objects or integer for days.
        # The following statements yield the same filtering criteria
        self.future_sp500.set_filter(timedelta(0), timedelta(182))
        self.future_gold.set_filter(0, 182)

        benchmark = self.add_equity("SPY")
        self.set_benchmark(benchmark.symbol)

        seeder = FuncSecuritySeeder(self.get_last_known_prices)
        self.set_security_initializer(lambda security: seeder.seed_security(security))

    def on_data(self,slice):
        if not self.portfolio.invested:
            for chain in slice.future_chains:
                 # Get contracts expiring no earlier than in 90 days
                contracts = list(filter(lambda x: x.expiry > self.time + timedelta(90), chain.value))

                # if there is any contract, trade the front contract
                if len(contracts) == 0: continue
                front = sorted(contracts, key = lambda x: x.expiry, reverse=True)[0]

                self.contract_symbol = front.symbol
                self.market_order(front.symbol , 1)
        else:
            self.liquidate()

    def on_end_of_algorithm(self):
        # Get the margin requirements
        buying_power_model = self.securities[self.contract_symbol].buying_power_model
        name = type(buying_power_model).__name__
        if name != 'FutureMarginModel':
            raise Exception(f"Invalid buying power model. Found: {name}. Expected: FutureMarginModel")

        initial_overnight = buying_power_model.initial_overnight_margin_requirement
        maintenance_overnight = buying_power_model.maintenance_overnight_margin_requirement
        initial_intraday = buying_power_model.initial_intraday_margin_requirement
        maintenance_intraday = buying_power_model.maintenance_intraday_margin_requirement

    def on_securities_changed(self, changes):
        for added_security in changes.added_securities:
            if added_security.symbol.security_type == SecurityType.FUTURE and not added_security.symbol.is_canonical() and not added_security.has_data:
                raise Exception(f"Future contracts did not work up as expected: {added_security.symbol}")
