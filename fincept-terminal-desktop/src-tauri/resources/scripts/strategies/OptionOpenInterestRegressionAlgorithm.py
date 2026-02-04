# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0FFCB469
# Category: Options
# Description: Options Open Interest data regression test
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Options Open Interest data regression test.
### </summary>
### <meta name="tag" content="options" />
### <meta name="tag" content="regression test" />
class OptionOpenInterestRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_cash(1000000)
        self.set_start_date(2014,6,5)
        self.set_end_date(2014,6,6)

        option = self.add_option("TWX")

        # set our strike/expiry filter for this option chain
        option.set_filter(-10, 10, timedelta(0), timedelta(365*2))

        # use the underlying equity as the benchmark
        self.set_benchmark("TWX")

    def on_data(self, slice):
        if not self.portfolio.invested: 
            for chain in slice.option_chains:
                for contract in chain.value:
                    if float(contract.symbol.id.strike_price) == 72.5 and \
                       contract.symbol.id.option_right == OptionRight.CALL and \
                       contract.symbol.id.date == datetime(2016, 1, 15):

                        history = self.history(OpenInterest, contract.symbol, timedelta(1))["openinterest"]
                        if len(history.index) == 0 or 0 in history.values:
                            raise ValueError("Regression test failed: open interest history request is empty")

                        security = self.securities[contract.symbol]
                        open_interest_cache = security.cache.get_data[OpenInterest]()
                        if open_interest_cache == None:
                            raise ValueError("Regression test failed: current open interest isn't in the security cache")
                        if slice.time.date() == datetime(2014, 6, 5).date() and (contract.open_interest != 50 or security.open_interest != 50):
                            raise ValueError("Regression test failed: current open interest was not correctly loaded and is not equal to 50")  
                        if slice.time.date() == datetime(2014, 6, 6).date() and (contract.open_interest != 70 or security.open_interest != 70):
                            raise ValueError("Regression test failed: current open interest was not correctly loaded and is not equal to 70")  
                        if slice.time.date() == datetime(2014, 6, 6).date():
                            self.market_order(contract.symbol, 1)
                            self.market_on_close_order(contract.symbol, -1)

    def on_order_event(self, order_event):
        self.log(str(order_event))
