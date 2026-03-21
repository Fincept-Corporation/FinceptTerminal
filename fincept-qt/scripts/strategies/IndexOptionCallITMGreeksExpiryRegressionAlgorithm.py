# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-32C0EE95
# Category: Options
# Description: This regression algorithm tests In The Money (ITM) index option expiry for calls. We test to make sure that index opt...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests In The Money (ITM) index option expiry for calls.
### We test to make sure that index options have greeks enabled, same as equity options.
### </summary>
class IndexOptionCallITMGreeksExpiryRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.on_data_calls = 0
        self.invested = False

        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 1, 31)

        spx = self.add_index("SPX", Resolution.MINUTE)
        spx.volatility_model = StandardDeviationOfReturnsVolatilityModel(60, Resolution.MINUTE, timedelta(minutes=1))
        self.spx = spx.symbol

        # Select a index option call expiring ITM, and adds it to the algorithm.
        self.spx_option = list(self.option_chain(self.spx))
        self.spx_option = [i for i in self.spx_option if i.id.strike_price <= 3200 and i.id.option_right == OptionRight.CALL and i.id.date.year == 2021 and i.id.date.month == 1]
        self.spx_option = list(sorted(self.spx_option, key=lambda x: x.id.strike_price, reverse=True))[0]
        self.spx_option = self.add_index_option_contract(self.spx_option, Resolution.MINUTE)

        self.spx_option.price_model = OptionPriceModels.black_scholes()

        self.expected_option_contract = Symbol.create_option(self.spx, Market.USA, OptionStyle.EUROPEAN, OptionRight.CALL, 3200, datetime(2021, 1, 15))
        if self.spx_option.symbol != self.expected_option_contract:
            raise Exception(f"Contract {self.expected_option_contract} was not found in the chain")

    def on_data(self, data: Slice):
        # Let the algo warmup, but without using SetWarmup. Otherwise, we get
        # no contracts in the option chain
        if self.invested or self.on_data_calls < 40:
            self.on_data_calls += 1
            return

        self.on_data_calls += 1

        if data.option_chains.count == 0:
            return

        if all([any([c.symbol not in data for c in o.contracts.values()]) for o in data.option_chains.values()]):
            return

        if len(list(list(data.option_chains.values())[0].contracts.values())) == 0:
            raise Exception(f"No contracts found in the option {list(data.option_chains.keys())[0]}")

        deltas = [i.greeks.delta for i in self.sort_by_max_volume(data)]
        gammas = [i.greeks.gamma for i in self.sort_by_max_volume(data)] #data.option_chains.values().order_by_descending(y => y.contracts.values().sum(x => x.volume)).first().contracts.values().select(x => x.greeks.gamma).to_list()
        lambda_ = [i.greeks.lambda_ for i in self.sort_by_max_volume(data)] #data.option_chains.values().order_by_descending(y => y.contracts.values().sum(x => x.volume)).first().contracts.values().select(x => x.greeks.lambda).to_list()
        rho = [i.greeks.rho for i in self.sort_by_max_volume(data)] #data.option_chains.values().order_by_descending(y => y.contracts.values().sum(x => x.volume)).first().contracts.values().select(x => x.greeks.rho).to_list()
        theta = [i.greeks.theta for i in self.sort_by_max_volume(data)] #data.option_chains.values().order_by_descending(y => y.contracts.values().sum(x => x.volume)).first().contracts.values().select(x => x.greeks.theta).to_list()
        vega = [i.greeks.vega for i in self.sort_by_max_volume(data)] #data.option_chains.values().order_by_descending(y => y.contracts.values().sum(x => x.volume)).first().contracts.values().select(x => x.greeks.vega).to_list()

        # The commented out test cases all return zero.
        # This is because of failure to evaluate the greeks in the option pricing model, most likely
        # due to us not clearing the default 30 day requirement for the volatility model to start being updated.
        if any([i for i in deltas if i == 0]):
            raise Exception("Option contract Delta was equal to zero")

        # Delta is 1, therefore we expect a gamma of 0
        if any([i for i in gammas if i == 0]):
            raise AggregateException("Option contract Gamma was equal to zero")

        if any([i for i in lambda_ if lambda_ == 0]):
            raise AggregateException("Option contract Lambda was equal to zero")

        if any([i for i in rho if i == 0]):
            raise Exception("Option contract Rho was equal to zero")

        if any([i for i in theta if i == 0]):
            raise Exception("Option contract Theta was equal to zero")

        # The strike is far away from the underlying asset's price, and we're very close to expiry.
        # Zero is an expected value here.
        if any([i for i in vega if vega == 0]):
            raise AggregateException("Option contract Vega was equal to zero")

        if not self.invested:
            self.set_holdings(list(list(data.option_chains.values())[0].contracts.values())[0].symbol, 1)
            self.invested = True

    ### <summary>
    ### Ran at the end of the algorithm to ensure the algorithm has no holdings
    ### </summary>
    ### <exception cref="Exception">The algorithm has holdings</exception>
    def on_end_of_algorithm(self):
        if self.portfolio.invested:
            raise Exception(f"Expected no holdings at end of algorithm, but are invested in: {', '.join(self.portfolio.keys())}")

        if not self.invested:
            raise Exception(f"Never checked greeks, maybe we have no option data?")

    def sort_by_max_volume(self, data: Slice):
        chain = [i for i in sorted(list(data.option_chains.values()), key=lambda x: sum([j.volume for j in x.contracts.values()]), reverse=True)][0]
        return chain.contracts.values()
