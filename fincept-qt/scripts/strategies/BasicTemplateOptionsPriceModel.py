# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-79BA1C77
# Category: Options
# Description: Example demonstrating how to define an option price model
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example demonstrating how to define an option price model.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="options" />
### <meta name="tag" content="filter selection" />
### <meta name="tag" content="option price model" />
class BasicTemplateOptionsPriceModel(QCAlgorithm):
    '''Example demonstrating how to define an option price model.'''

    def initialize(self):
        self.set_start_date(2020, 1, 1)
        self.set_end_date(2020, 1, 5)
        self.set_cash(100000)

        # Add the option
        option = self.add_option("AAPL")
        self.option_symbol = option.symbol

        # Add the initial contract filter
        option.set_filter(-3, +3, 0, 31)

        # Define the Option Price Model
        option.price_model = OptionPriceModels.crank_nicolson_fd()
        #option.price_model = OptionPriceModels.black_scholes()
        #option.price_model = OptionPriceModels.additive_equiprobabilities()
        #option.price_model = OptionPriceModels.barone_adesi_whaley()
        #option.price_model = OptionPriceModels.binomial_cox_ross_rubinstein()
        #option.price_model = OptionPriceModels.binomial_jarrow_rudd()
        #option.price_model = OptionPriceModels.binomial_joshi()
        #option.price_model = OptionPriceModels.binomial_leisen_reimer()
        #option.price_model = OptionPriceModels.binomial_tian()
        #option.price_model = OptionPriceModels.binomial_trigeorgis()
        #option.price_model = OptionPriceModels.bjerksund_stensland()
        #option.price_model = OptionPriceModels.integral()

        # Set warm up with 30 trading days to warm up the underlying volatility model
        self.set_warm_up(30, Resolution.DAILY)


    def on_data(self,slice):
        '''OnData will test whether the option contracts has a non-zero Greeks.delta'''

        if self.is_warming_up or not slice.option_chains.contains_key(self.option_symbol):
            return

        chain = slice.option_chains[self.option_symbol]
        if not any([x for x in chain if x.greeks.delta != 0]):
            self.log(f'No contract with Delta != 0')
