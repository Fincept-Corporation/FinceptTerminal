# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9F0275D6
# Category: Options
# Description: Example demonstrating how to access to options history for a given underlying equity security
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example demonstrating how to access to options history for a given underlying equity security.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="options" />
### <meta name="tag" content="filter selection" />
### <meta name="tag" content="history" />
class BasicTemplateOptionsHistoryAlgorithm(QCAlgorithm):
    ''' This example demonstrates how to get access to options history for a given underlying equity security.'''

    def initialize(self):
        # this test opens position in the first day of trading, lives through stock split (7 for 1), and closes adjusted position on the second day
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(1000000)

        option = self.add_option("GOOG")
        # add the initial contract filter 
        # SetFilter method accepts timedelta objects or integer for days.
        # The following statements yield the same filtering criteria
        option.set_filter(-2, +2, 0, 180)
        # option.set_filter(-2,2, timedelta(0), timedelta(180))

        # set the pricing model for Greeks and volatility
        # find more pricing models https://www.quantconnect.com/lean/documentation/topic27704.html
        option.price_model = OptionPriceModels.crank_nicolson_fd()
        # set the warm-up period for the pricing model
        self.set_warm_up(TimeSpan.from_days(4))
        # set the benchmark to be the initial cash
        self.set_benchmark(lambda x: 1000000)

    def on_data(self,slice):
        if self.is_warming_up: return
        if not self.portfolio.invested:
            for chain in slice.option_chains:
                volatility = self.securities[chain.key.underlying].volatility_model.volatility
                for contract in chain.value:
                    self.log("{0},Bid={1} Ask={2} Last={3} OI={4} sigma={5:.3f} NPV={6:.3f} \
                              delta={7:.3f} gamma={8:.3f} vega={9:.3f} beta={10:.2f} theta={11:.2f} IV={12:.2f}".format(
                    contract.symbol.value,
                    contract.bid_price,
                    contract.ask_price,
                    contract.last_price,
                    contract.open_interest,
                    volatility,
                    contract.theoretical_price,
                    contract.greeks.delta,
                    contract.greeks.gamma,
                    contract.greeks.vega,
                    contract.greeks.rho,
                    contract.greeks.theta / 365,
                    contract.implied_volatility))

    def on_securities_changed(self, changes):
        for change in changes.added_securities:
            # only print options price
            if change.symbol.value == "GOOG": return
            history = self.history(change.symbol, 10, Resolution.MINUTE).sort_index(level='time', ascending=False)[:3]
            for index, row in history.iterrows():
                self.log("History: " + str(index[3])
                        + ": " + index[4].strftime("%m/%d/%Y %I:%M:%S %p")
                        + " > " + str(row.close))
