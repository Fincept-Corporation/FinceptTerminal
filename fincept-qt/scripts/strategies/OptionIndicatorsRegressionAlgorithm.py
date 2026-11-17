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

class OptionIndicatorsRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 6, 5)
        self.set_end_date(2014, 6, 7)
        self.set_cash(100000)

        self.add_equity("AAPL", Resolution.MINUTE)
        option = Symbol.create_option("AAPL", Market.USA, OptionStyle.AMERICAN, OptionRight.PUT, 505, datetime(2014, 6, 27))
        self.add_option_contract(option, Resolution.MINUTE)

        self.implied_volatility = self.iv(option, option_model = OptionPricingModelType.BLACK_SCHOLES)
        self.delta = self.d(option, option_model = OptionPricingModelType.BINOMIAL_COX_ROSS_RUBINSTEIN, iv_model = OptionPricingModelType.BLACK_SCHOLES)
        self.gamma = self.g(option, option_model = OptionPricingModelType.FORWARD_TREE, iv_model = OptionPricingModelType.BLACK_SCHOLES)
        self.vega = self.v(option, option_model = OptionPricingModelType.FORWARD_TREE, iv_model = OptionPricingModelType.BLACK_SCHOLES)
        self.theta = self.t(option, option_model = OptionPricingModelType.FORWARD_TREE, iv_model = OptionPricingModelType.BLACK_SCHOLES)
        self.rho = self.r(option, option_model = OptionPricingModelType.FORWARD_TREE, iv_model = OptionPricingModelType.BLACK_SCHOLES)

    def on_end_of_algorithm(self):
        if self.implied_volatility.current.value == 0 or self.delta.current.value == 0 or self.gamma.current.value == 0 \
        or self.vega.current.value == 0 or self.theta.current.value == 0 or self.rho.current.value == 0:
            raise Exception("Expected IV/greeks calculated")

        self.debug(f"""Implied Volatility: {self.implied_volatility.current.value},
Delta: {self.delta.current.value},
Gamma: {self.gamma.current.value},
Vega: {self.vega.current.value},
Theta: {self.theta.current.value},
Rho: {self.rho.current.value}""")
