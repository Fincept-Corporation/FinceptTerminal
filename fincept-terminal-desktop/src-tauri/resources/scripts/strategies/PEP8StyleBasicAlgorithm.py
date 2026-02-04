# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-73AEBCB1
# Category: General Strategy
# Description: P E P8 Style Basic Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class PEP8StyleBasicAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10,11)
        self.set_cash(100000)

        self.spy = self.add_equity("SPY", Resolution.MINUTE, extended_market_hours=False, fill_forward=True).symbol

        # Test accessing a constant (QCAlgorithm.MaxTagsCount)
        self.debug("MaxTagsCount: " + str(self.MAX_TAGS_COUNT))

    def on_data(self, slice):
        if not self.portfolio.invested:
            self.set_holdings(self.spy, 1)
            self.debug("Purchased Stock")

    def on_order_event(self, order_event):
        self.log(f"{self.time} :: {order_event}")

    def on_end_of_algorithm(self):
        self.log("Algorithm ended!")

        if not self.portfolio.invested:
            raise Exception("Algorithm should have been invested at the end of the algorithm")

        # let's do some logging to do more pep8 style testing
        self.log("-----------------------------------------------------------------------------------------")
        self.log(f"{self.spy.value} last price: {self.securities[self.spy].price}")
        self.log(f"{self.spy.value} holdings: "
                 f"{self.securities[self.spy].holdings.quantity}@{self.securities[self.spy].holdings.price}="
                 f"{self.securities[self.spy].holdings.holdings_value}")
        self.log("-----------------------------------------------------------------------------------------")
