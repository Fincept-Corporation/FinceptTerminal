# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-119E482D
# Category: Futures
# Description: Future Stop Market Order On Extended Hours Regression Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from QuantConnect import Orders

# <summary>
# This example demonstrates how to create future 'stop_market_order' in extended Market Hours time
# </summary>

class FutureStopMarketOrderOnExtendedHoursRegressionAlgorithm(QCAlgorithm):
    # Keep new created instance of stop_market_order
    stop_market_ticket = None
    sp_500_e_mini = None

    # Initialize the Algorithm and Prepare Required Data
    def initialize(self):
        self.set_start_date(2013, 10, 6)
        self.set_end_date(2013, 10, 12)

        # Add mini SP500 future with extended Market hours flag
        self.sp_500_e_mini = self.add_future(Futures.Indices.SP_500_E_MINI, Resolution.MINUTE, extended_market_hours=True)

        # Init new schedule event with params: every_day, 19:00:00 PM, what should to do
        self.schedule.on(self.date_rules.every_day(),self.time_rules.at(19, 0),self.make_market_and_stop_market_order)

    # This method is opened 2 new orders by scheduler
    def make_market_and_stop_market_order(self):
        self.market_order(self.sp_500_e_mini.mapped, 1)
        self.stop_market_ticket = self.stop_market_order(self.sp_500_e_mini.mapped, -1, self.sp_500_e_mini.price * 1.1)

    # New Data Event handler receiving all subscription data in a single event
    def on_data(self, slice):
        if (self.stop_market_ticket == None or self.stop_market_ticket.status != OrderStatus.SUBMITTED):
            return None

        self.stop_price = self.stop_market_ticket.get(OrderField.STOP_PRICE)
        self.bar = self.securities[self.stop_market_ticket.symbol].cache.get_data()

    # An order fill update the resulting information is passed to this method.
    def on_order_event(self, order_event):
        if order_event is None:
            return None

        if self.transactions.get_order_by_id(order_event.order_id).type is not OrderType.STOP_MARKET:
            return None

        if order_event.status == OrderStatus.FILLED:
            # Get Exchange Hours for specific security
            exchange_hours = self.market_hours_database.get_exchange_hours(self.sp_500_e_mini.subscription_data_config)

            # Validate, Exchange is opened explicitly
            if (not exchange_hours.is_open(order_event.utc_time, self.sp_500_e_mini.is_extended_market_hours)):
                raise Exception("The Exchange hours was closed, verify 'extended_market_hours' flag in Initialize() when added new security(ies)")

    def on_end_of_algorithm(self):
        self.stop_market_orders = self.transactions.get_orders(lambda o: o.type is OrderType.STOP_MARKET)

        for o in self.stop_market_orders:
            if o.status != OrderStatus.FILLED:
                raise Exception("The Algorithms was not handled any StopMarketOrders")
