# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-BFC09E5D
# Category: Regression Test
# Description: Provides a regression baseline focused on updating orders
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from math import copysign

### <summary>
### Provides a regression baseline focused on updating orders
### </summary>
### <meta name="tag" content="regression test" />
class UpdateOrderRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,1,1)    #Set Start Date
        self.set_end_date(2015,1,1)      #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration

        self.security = self.add_equity("SPY", Resolution.DAILY)

        self.last_month = -1
        self.quantity = 100
        self.delta_quantity = 10

        self.stop_percentage = 0.025
        self.stop_percentage_delta = 0.005
        self.limit_percentage = 0.025
        self.limit_percentage_delta = 0.005

        order_type_enum = [OrderType.MARKET, OrderType.LIMIT, OrderType.STOP_MARKET, OrderType.STOP_LIMIT, OrderType.MARKET_ON_OPEN, OrderType.MARKET_ON_CLOSE, OrderType.TRAILING_STOP]
        self.order_types_queue = CircularQueue[OrderType](order_type_enum)
        self.order_types_queue.circle_completed += self.on_circle_completed
        self.tickets = []


    def on_circle_completed(self, sender, event):
        '''Flip our signs when we've gone through all the order types'''
        self.quantity *= -1


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if not data.bars.contains_key("SPY"):
            return

        if self.time.month != self.last_month:
            # we'll submit the next type of order from the queue
            order_type = self.order_types_queue.dequeue()
            #Log("")
            self.Log("\r\n--------------MONTH: {0}:: {1}".format(self.time.strftime("%B"), order_type))
            #Log("")
            self.last_month = self.time.month
            self.log("ORDER TYPE:: {0}".format(order_type))
            is_long = self.quantity > 0
            stop_price = (1 + self.stop_percentage)*data["SPY"].high if is_long else (1 - self.stop_percentage)*data["SPY"].low
            limit_price = (1 - self.limit_percentage)*stop_price if is_long else (1 + self.limit_percentage)*stop_price

            if order_type == OrderType.LIMIT:
                limit_price = (1 + self.limit_percentage)*data["SPY"].high if not is_long else (1 - self.limit_percentage)*data["SPY"].low

            request = SubmitOrderRequest(order_type, self.security.symbol.security_type, "SPY", self.quantity, stop_price, limit_price, 0, 0.01, True,
                                         self.utc_time, str(order_type))
            ticket = self.transactions.add_order(request)
            self.tickets.append(ticket)

        elif len(self.tickets) > 0:
            ticket = self.tickets[-1]

            if self.time.day > 8 and self.time.day < 14:
                if len(ticket.update_requests) == 0 and ticket.status is not OrderStatus.FILLED:
                    self.log("TICKET:: {0}".format(ticket))
                    update_order_fields = UpdateOrderFields()
                    update_order_fields.quantity = ticket.quantity + copysign(self.delta_quantity, self.quantity)
                    update_order_fields.tag = "Change quantity: {0}".format(self.time.day)
                    ticket.update(update_order_fields)

            elif self.time.day > 13 and self.time.day < 20:
                if len(ticket.update_requests) == 1 and ticket.status is not OrderStatus.FILLED:
                    self.log("TICKET:: {0}".format(ticket))
                    update_order_fields = UpdateOrderFields()
                    update_order_fields.limit_price = self.security.price*(1 - copysign(self.limit_percentage_delta, ticket.quantity))
                    update_order_fields.stop_price = self.security.price*(1 + copysign(self.stop_percentage_delta, ticket.quantity)) if ticket.order_type != OrderType.TRAILING_STOP else None
                    update_order_fields.tag = "Change prices: {0}".format(self.time.day)
                    ticket.update(update_order_fields)
            else:
                if len(ticket.update_requests) == 2 and ticket.status is not OrderStatus.FILLED:
                    self.log("TICKET:: {0}".format(ticket))
                    ticket.cancel("{0} and is still open!".format(self.time.day))
                    self.log("CANCELLED:: {0}".format(ticket.cancel_request))


    def on_order_event(self, orderEvent):
        order = self.transactions.get_order_by_id(orderEvent.order_id)
        ticket = self.transactions.get_order_ticket(orderEvent.order_id)

        #order cancelations update CanceledTime
        if order.status == OrderStatus.CANCELED and order.canceled_time != orderEvent.utc_time:
            raise ValueError("Expected canceled order CanceledTime to equal canceled order event time.")

        #fills update LastFillTime
        if (order.status == OrderStatus.FILLED or order.status == OrderStatus.PARTIALLY_FILLED) and order.last_fill_time != orderEvent.utc_time:
            raise ValueError("Expected filled order LastFillTime to equal fill order event time.")

        # check the ticket to see if the update was successfully processed
        if len([ur for ur in ticket.update_requests if ur.response is not None and ur.response.is_success]) > 0 and order.created_time != self.utc_time and order.last_update_time is None:
            raise ValueError("Expected updated order LastUpdateTime to equal submitted update order event time")

        if orderEvent.status == OrderStatus.FILLED:
            self.log("FILLED:: {0} FILL PRICE:: {1}".format(self.transactions.get_order_by_id(orderEvent.order_id), orderEvent.fill_price))
        else:
            self.log(orderEvent.to_string())
            self.log("TICKET:: {0}".format(ticket))
