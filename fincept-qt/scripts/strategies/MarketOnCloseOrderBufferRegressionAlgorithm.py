# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-74AD11AC
# Category: Regression Test
# Description: Market On Close Order Buffer Regression Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class MarketOnCloseOrderBufferRegressionAlgorithm(QCAlgorithm):
    valid_order_ticket = None
    invalid_order_ticket = None

    def initialize(self):
        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,8)    #Set End Date

        self.add_equity("SPY", Resolution.MINUTE)

        def moc_at_post_market():
            self.valid_order_ticket_extended_market_hours = self.market_on_close_order("SPY", 2)

        self.schedule.on(self.date_rules.today, self.time_rules.at(17,0), moc_at_post_market)
                
        # Modify our submission buffer time to 10 minutes
        MarketOnCloseOrder.submission_time_buffer = timedelta(minutes=10)

    def on_data(self, data):
        # Test our ability to submit MarketOnCloseOrders
        # Because we set our buffer to 10 minutes, any order placed
        # before 3:50PM should be accepted, any after marked invalid

        # Will not throw an order error and execute
        if self.time.hour == 15 and self.time.minute == 49 and not self.valid_order_ticket:
            self.valid_order_ticket = self.market_on_close_order("SPY", 2)

        # Will throw an order error and be marked invalid
        if self.time.hour == 15 and self.time.minute == 51 and not self.invalid_order_ticket:
            self.invalid_order_ticket = self.market_on_close_order("SPY", 2)

    def on_end_of_algorithm(self):
        # Set it back to default for other regressions
        MarketOnCloseOrder.submission_time_buffer = MarketOnCloseOrder.DEFAULT_SUBMISSION_TIME_BUFFER

        if self.valid_order_ticket.status != OrderStatus.FILLED:
            raise Exception("Valid order failed to fill")

        if self.invalid_order_ticket.status != OrderStatus.INVALID:
            raise Exception("Invalid order was not rejected")

        if self.valid_order_ticket_extended_market_hours.status != OrderStatus.FILLED:
            raise Exception("Valid order during extended market hours failed to fill")
