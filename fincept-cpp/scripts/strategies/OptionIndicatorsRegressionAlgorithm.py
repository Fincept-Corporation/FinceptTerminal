# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-707335FB
# Category: Options
# Description: Option Indicators Regression Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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
