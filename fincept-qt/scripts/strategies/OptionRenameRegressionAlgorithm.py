# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-8E5F0BE5
# Category: Options
# Description: This is an option split regression algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This is an option split regression algorithm
### </summary>
### <meta name="tag" content="options" />
### <meta name="tag" content="regression test" />
class OptionRenameRegressionAlgorithm(QCAlgorithm):

    def initialize(self):

        self.set_cash(1000000)
        self.set_start_date(2013,6,28)
        self.set_end_date(2013,7,2)
        option = self.add_option("TFCFA")

        # set our strike/expiry filter for this option chain
        option.set_filter(-1, 1, timedelta(0), timedelta(3650))
        # use the underlying equity as the benchmark
        self.set_benchmark("TFCFA")

    def on_data(self, slice):
        ''' Event - v3.0 DATA EVENT HANDLER: (Pattern) Basic template for user to override for receiving all subscription data in a single event
        <param name="slice">The current slice of data keyed by symbol string</param> '''
        if not self.portfolio.invested: 
            for kvp in slice.option_chains:
                chain = kvp.value
                if self.time.day == 28 and self.time.hour > 9 and self.time.minute > 0:
    
                    contracts = [i for i in sorted(chain, key=lambda x:x.expiry) 
                                         if i.right ==  OptionRight.CALL and 
                                            i.strike == 33 and
                                            i.expiry.date() == datetime(2013,8,17).date()]
                    if contracts:
                        # Buying option
                        contract = contracts[0]
                        self.buy(contract.symbol, 1)
                        # Buy the undelying stock
                        underlying_symbol = contract.symbol.underlying
                        self.buy (underlying_symbol, 100)
                        # check
                        if float(contract.ask_price) != 1.1:
                            raise ValueError("Regression test failed: current ask price was not loaded from NWSA backtest file and is not $1.1")
        elif self.time.day == 2 and self.time.hour > 14 and self.time.minute > 0:
            for kvp in slice.option_chains:
                chain = kvp.value
                self.liquidate()
                contracts = [i for i in sorted(chain, key=lambda x:x.expiry) 
                                        if i.right ==  OptionRight.CALL and 
                                           i.strike == 33 and
                                           i.expiry.date() == datetime(2013,8,17).date()]
            if contracts:
                contract = contracts[0]
                self.log("Bid Price" + str(contract.bid_price))
                if float(contract.bid_price) != 0.05:
                    raise ValueError("Regression test failed: current bid price was not loaded from FOXA file and is not $0.05")

    def on_order_event(self, order_event):
        self.log(str(order_event))
