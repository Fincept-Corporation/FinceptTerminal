# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-F8E8016B
# Category: General Strategy
# Description: Demonstration algorithm of time in force order settings
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration algorithm of time in force order settings.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class TimeInForceAlgorithm(QCAlgorithm):

    # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    def initialize(self):

        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)
        self.set_cash(100000)

        # The default time in force setting for all orders is GoodTilCancelled (GTC),
        # uncomment this line to set a different time in force.
        # We currently only support GTC and DAY.
        # self.default_order_properties.time_in_force = TimeInForce.day

        self.symbol = self.add_equity("SPY", Resolution.MINUTE).symbol

        self.gtc_order_ticket1 = None
        self.gtc_order_ticket2 = None
        self.day_order_ticket1 = None
        self.day_order_ticket2 = None
        self.gtd_order_ticket1 = None
        self.gtd_order_ticket2 = None
        self.expected_order_statuses = {}

    # OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    # Arguments:
    #    data: Slice object keyed by symbol containing the stock data
    def on_data(self, data):

        if self.gtc_order_ticket1 is None:
            # These GTC orders will never expire and will not be canceled automatically.

            self.default_order_properties.time_in_force = TimeInForce.GOOD_TIL_CANCELED

            # this order will not be filled before the end of the backtest
            self.gtc_order_ticket1 = self.limit_order(self.symbol, 10, 100)
            self.expected_order_statuses[self.gtc_order_ticket1.order_id] = OrderStatus.SUBMITTED

            # this order will be filled before the end of the backtest
            self.gtc_order_ticket2 = self.limit_order(self.symbol, 10, 160)
            self.expected_order_statuses[self.gtc_order_ticket2.order_id] = OrderStatus.FILLED

        if self.day_order_ticket1 is None:
            # These DAY orders will expire at market close,
            # if not filled by then they will be canceled automatically.

            self.default_order_properties.time_in_force = TimeInForce.DAY

            # this order will not be filled before market close and will be canceled
            self.day_order_ticket1 = self.limit_order(self.symbol, 10, 140)
            self.expected_order_statuses[self.day_order_ticket1.order_id] = OrderStatus.CANCELED

            # this order will be filled before market close
            self.day_order_ticket2 = self.limit_order(self.symbol, 10, 180)
            self.expected_order_statuses[self.day_order_ticket2.order_id] = OrderStatus.FILLED

        if self.gtd_order_ticket1 is None:
            # These GTD orders will expire on October 10th at market close,
            # if not filled by then they will be canceled automatically.

            self.default_order_properties.time_in_force = TimeInForce.good_til_date(datetime(2013, 10, 10))

            # this order will not be filled before expiry and will be canceled
            self.gtd_order_ticket1 = self.limit_order(self.symbol, 10, 100)
            self.expected_order_statuses[self.gtd_order_ticket1.order_id] = OrderStatus.CANCELED

            # this order will be filled before expiry
            self.gtd_order_ticket2 = self.limit_order(self.symbol, 10, 160)
            self.expected_order_statuses[self.gtd_order_ticket2.order_id] = OrderStatus.FILLED

    # Order event handler. This handler will be called for all order events, including submissions, fills, cancellations.
    # This method can be called asynchronously, ensure you use proper locks on thread-unsafe objects
    def on_order_event(self, orderEvent):
        self.debug(f"{self.time} {orderEvent}")

    # End of algorithm run event handler. This method is called at the end of a backtest or live trading operation.
    def on_end_of_algorithm(self):
        for orderId, expectedStatus in self.expected_order_statuses.items():
            order = self.transactions.get_order_by_id(orderId)
            if order.status != expectedStatus:
                raise Exception(f"Invalid status for order {orderId} - Expected: {expectedStatus}, actual: {order.status}")
