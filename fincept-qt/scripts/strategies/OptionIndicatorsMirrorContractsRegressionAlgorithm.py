# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-CFC8C48B
# Category: Options
# Description: Option Indicators Mirror Contracts Regression Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from scipy.optimize import brentq

class OptionIndicatorsMirrorContractsRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 6, 5)
        self.set_end_date(2014, 6, 9)
        self.set_cash(100000)

        equity = self.add_equity("AAPL", Resolution.DAILY).symbol
        option = Symbol.create_option("AAPL", Market.USA, OptionStyle.AMERICAN, OptionRight.PUT, 650, datetime(2014, 6, 21))
        self.add_option_contract(option, Resolution.DAILY)
        # add the call counter side of the mirrored pair
        mirror_option = Symbol.create_option("AAPL", Market.USA, OptionStyle.AMERICAN, OptionRight.CALL, 650, datetime(2014, 6, 21))
        self.add_option_contract(mirror_option, Resolution.DAILY)

        self.delta = self.d(option, mirror_option, option_model = OptionPricingModelType.BINOMIAL_COX_ROSS_RUBINSTEIN, iv_model = OptionPricingModelType.BLACK_SCHOLES)
        self.gamma = self.g(option, mirror_option, option_model = OptionPricingModelType.FORWARD_TREE, iv_model = OptionPricingModelType.BLACK_SCHOLES)
        self.vega = self.v(option, mirror_option, option_model = OptionPricingModelType.FORWARD_TREE, iv_model = OptionPricingModelType.BLACK_SCHOLES)
        self.theta = self.t(option, mirror_option, option_model = OptionPricingModelType.FORWARD_TREE, iv_model = OptionPricingModelType.BLACK_SCHOLES)
        self.rho = self.r(option, mirror_option, option_model = OptionPricingModelType.FORWARD_TREE, iv_model = OptionPricingModelType.BLACK_SCHOLES)

        # A custom IV indicator with custom calculation of IV
        risk_free_rate_model = InterestRateProvider()
        dividend_yield_model = DividendYieldProvider(equity)
        self.implied_volatility = CustomImpliedVolatility(option, mirror_option, risk_free_rate_model, dividend_yield_model)
        self.register_indicator(option, self.implied_volatility, QuoteBarConsolidator(timedelta(1)))
        self.register_indicator(mirror_option, self.implied_volatility, QuoteBarConsolidator(timedelta(1)))
        self.register_indicator(equity, self.implied_volatility, TradeBarConsolidator(timedelta(1)))

        # custom IV smoothing function: assume the lower IV is more "fair"
        smoothing_func = lambda iv, mirror_iv: min(iv, mirror_iv)
        # set the smoothing function
        self.delta.implied_volatility.set_smoothing_function(smoothing_func)
        self.gamma.implied_volatility.set_smoothing_function(smoothing_func)
        self.vega.implied_volatility.set_smoothing_function(smoothing_func)
        self.theta.implied_volatility.set_smoothing_function(smoothing_func)
        self.rho.implied_volatility.set_smoothing_function(smoothing_func)

    def on_end_of_algorithm(self):
        if not self.implied_volatility.is_ready or not self.delta.is_ready or not self.gamma.is_ready \
        or not self.vega.is_ready or not self.theta.is_ready or not self.rho.is_ready:
            raise Exception("Expected IV/greeks calculated")

        self.debug(f"""Implied Volatility: {self.implied_volatility.current.value},
Delta: {self.delta.current.value},
Gamma: {self.gamma.current.value},
Vega: {self.vega.current.value},
Theta: {self.theta.current.value},
Rho: {self.rho.current.value}""")

class CustomImpliedVolatility(ImpliedVolatility):
    def __init__(self, option, mirror_option, risk_free_rate_model, dividend_yield_model):
        super().__init__(option, risk_free_rate_model, dividend_yield_model, mirror_option)
        self.set_smoothing_function(lambda iv, mirror_iv: iv)

    def calculate_iv(self, time_till_expiry: float) -> float:
        try:
            return brentq(self.f, 1e-7, 2.0, args=(time_till_expiry), xtol=1e-4, maxiter=100)
        except:
            print("ImpliedVolatility.calculate_i_v(): Fail to converge, returning 0.")
            return 0.0

    # we demonstate put-call parity calculation here, but note that it is not suitable for American options
    def f(self, vol: float, time_till_expiry: float) -> float:
        call_black_price = OptionGreekIndicatorsHelper.black_theoretical_price(
            vol, UnderlyingPrice.current.value, self.strike, time_till_expiry, RiskFreeRate.current.value, DividendYield.current.value, OptionRight.CALL)
        put_black_price = OptionGreekIndicatorsHelper.black_theoretical_price(
            vol, UnderlyingPrice.current.value, self.strike, time_till_expiry, RiskFreeRate.current.value, DividendYield.current.value, OptionRight.PUT)
        return Price.current.value + OppositePrice.current.value - call_black_price - put_black_price
