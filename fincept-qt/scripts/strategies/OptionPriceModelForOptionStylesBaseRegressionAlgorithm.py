# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9520559C
# Category: Options
# Description: Base regression algorithm exercising different style options with option price models that might or might not support...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Base regression algorithm exercising different style options with option price models that might
### or might not support them. Also, if the option style is supported, greeks are asserted to be accesible and have valid values.
### </summary>
class OptionPriceModelForOptionStylesBaseRegressionAlgorithm(QCAlgorithm):
    def __init__(self):
        super().__init__()
        self._option_style_is_supported = False
        self._check_greeks = True
        self._tried_greeks_calculation = False
        self._option = None

    def on_data(self, slice):
        if self.is_warming_up: return

        for kvp in slice.option_chains:
            if self._option is None or kvp.key != self._option.symbol: continue

            self.check_greeks([contract for contract in kvp.value])

    def on_end_of_day(self, symbol):
        self._check_greeks = True

    def on_end_of_algorithm(self):
        if not self._tried_greeks_calculation:
            raise Exception("Expected greeks to be accessed")

    def init(self, option, option_style_is_supported):
        self._option = option
        self._option_style_is_supported = option_style_is_supported
        self._check_greeks = True
        self._tried_greeks_calculation = False

    def check_greeks(self, contracts):
        if not self._check_greeks or len(contracts) == 0: return

        self._check_greeks = False
        self._tried_greeks_calculation = True

        for contract in contracts:
            greeks = None
            try:
                greeks = contract.greeks

                # Greeks should have not been successfully accessed if the option style is not supported
                option_style_str = 'American' if self._option.style == OptionStyle.AMERICAN else 'European'
                if not self._option_style_is_supported:
                    raise Exception(f'Expected greeks not to be calculated for {contract.symbol.value}, an {option_style_str} style option, using {type(self._option.price_model).__name__}, which does not support them, but they were')
            except ArgumentException:
                # ArgumentException is only expected if the option style is not supported
                if self._option_style_is_supported:
                    raise Exception(f'Expected greeks to be calculated for {contract.symbol.value}, an {option_style_str} style option, using {type(self._option.price_model).__name__}, which supports them, but they were not')

            # Greeks should be valid if they were successfuly accessed for supported option style
            # Delta can be {-1, 0, 1} if the price is too wild, rho can be 0 if risk free rate is 0
            # Vega can be 0 if the price is very off from theoretical price, Gamma = 0 if Delta belongs to {-1, 1}
            if (self._option_style_is_supported
                and (greeks is None
                    or ((contract.right == OptionRight.CALL and (greeks.delta < 0.0 or greeks.delta > 1.0 or greeks.rho < 0.0))
                        or (contract.right == OptionRight.PUT and (greeks.delta < -1.0 or greeks.delta > 0.0 or greeks.rho > 0.0))
                        or greeks.theta == 0.0 or greeks.vega < 0.0 or greeks.gamma < 0.0))):
                raise Exception(f'Expected greeks to have valid values. Greeks were: Delta: {greeks.delta}, Rho: {greeks.rho}, Theta: {greeks.theta}, Vega: {greeks.vega}, Gamma: {greeks.gamma}')



