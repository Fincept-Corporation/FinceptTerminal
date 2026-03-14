# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-79957FB5
# Category: Regression Test
# Description: Regression algorithm reproducing GH issue #5921. Asserting a security can be warmup correctly on initialize
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm reproducing GH issue #5921. Asserting a security can be warmup correctly on initialize
### </summary>
class SecuritySeederRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10, 8)
        self.set_end_date(2013,10,10)

        self.set_security_initializer(BrokerageModelSecurityInitializer(self.brokerage_model,
                                                                        FuncSecuritySeeder(self.get_last_known_prices)))
        self.add_equity("SPY", Resolution.MINUTE)

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)

    def on_securities_changed(self, changes):
        for added_security in changes.added_securities:
            if not added_security.has_data \
                or added_security.ask_price == 0 \
                or added_security.bid_price == 0 \
                or added_security.bid_size == 0 \
                or added_security.ask_size == 0 \
                or added_security.price == 0 \
                or added_security.volume == 0 \
                or added_security.high == 0 \
                or added_security.low == 0 \
                or added_security.open == 0 \
                or added_security.close == 0:
                raise ValueError(f"Security {added_security.symbol} was not warmed up!")
