# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-4AD7FFC0
# Category: General Strategy
# Description: In this algorithm we submit/update/cancel each order type
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### In this algorithm we submit/update/cancel each order type
### </summary>
### <meta name="tag" content="trading and orders" />
### <meta name="tag" content="placing orders" />
### <meta name="tag" content="managing orders" />
### <meta name="tag" content="order tickets" />
### <meta name="tag" content="updating orders" />
class OrderTicketDemoAlgorithm(QCAlgorithm):
    '''In this algorithm we submit/update/cancel each order type'''
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        equity = self.add_equity("SPY")
        self.spy = equity.symbol

        self.__open_market_on_open_orders = []
        self.__open_market_on_close_orders = []
        self.__open_limit_orders = []
        self.__open_stop_market_orders = []
        self.__open_stop_limit_orders = []
        self.__open_trailing_stop_orders = []


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        # MARKET ORDERS
        self.market_orders()

        # LIMIT ORDERS
        self.limit_orders()

        # STOP MARKET ORDERS
        self.stop_market_orders()

        # STOP LIMIT ORDERS
        self.stop_limit_orders()

        # TRAILING STOP ORDERS
        self.trailing_stop_orders()

        # MARKET ON OPEN ORDERS
        self.market_on_open_orders()

        # MARKET ON CLOSE ORDERS
        self.market_on_close_orders()


    def market_orders(self):
        ''' MarketOrders are the only orders that are processed synchronously by default, so
        they'll fill by the next line of code. This behavior equally applies to live mode.
        You can opt out of this behavior by specifying the 'asynchronous' parameter as True.'''
        if self.time_is(7, 9, 31):
            self.log("Submitting MarketOrder")

            # submit a market order to buy 10 shares, this function returns an OrderTicket object
            # we submit the order with asynchronous = False, so it block until it is filled
            new_ticket = self.market_order(self.spy, 10, asynchronous = False)
            if new_ticket.status != OrderStatus.FILLED:
                self.log("Synchronous market order was not filled synchronously!")
                self.quit()

            # we can also submit the ticket asynchronously. In a backtest, we'll still perform the fill
            # before the next time events for your algorithm. here we'll submit the order asynchronously
            # and try to cancel it, sometimes it will, sometimes it will be filled first.
            new_ticket = self.market_order(self.spy, 10, asynchronous = True)
            response = new_ticket.cancel("Attempt to cancel async order")
            if response.is_success:
                self.log("Successfully canceled async market order: {0}".format(new_ticket.order_id))
            else:
                self.log("Unable to cancel async market order: {0}".format(response.error_code))


    def limit_orders(self):
        '''LimitOrders are always processed asynchronously. Limit orders are used to
        set 'good' entry points for an order. For example, you may wish to go
        long a stock, but want a good price, so can place a LimitOrder to buy with
        a limit price below the current market price. Likewise the opposite is True
        when selling, you can place a LimitOrder to sell with a limit price above the
        current market price to get a better sale price.
        You can submit requests to update or cancel the LimitOrder at any time.
        The 'LimitPrice' for an order can be retrieved from the ticket using the
        OrderTicket.get(OrderField) method, for example:
        Code:
            current_limit_price = order_ticket.get(OrderField.LIMIT_PRICE)'''
        if self.time_is(7, 12, 0):
            self.log("Submitting LimitOrder")

            # submit a limit order to buy 10 shares at .1% below the bar's close
            close = self.securities[self.spy.value].close
            new_ticket = self.limit_order(self.spy, 10, close * .999)
            self.__open_limit_orders.append(new_ticket)

            # submit another limit order to sell 10 shares at .1% above the bar's close
            new_ticket = self.limit_order(self.spy, -10, close * 1.001)
            self.__open_limit_orders.append(new_ticket)

        # when we submitted new limit orders we placed them into this list,
        # so while there's two entries they're still open and need processing
        if len(self.__open_limit_orders) == 2:
            open_orders = self.__open_limit_orders

            # check if either is filled and cancel the other
            long_order = open_orders[0]
            short_order = open_orders[1]
            if self.check_pair_orders_for_fills(long_order, short_order):
                self.__open_limit_orders = []
                return

            # if neither order has filled, bring in the limits by a penny
            new_long_limit = long_order.get(OrderField.LIMIT_PRICE) + 0.01
            new_short_limit = short_order.get(OrderField.LIMIT_PRICE) - 0.01
            self.log("Updating limits - Long: {0:.2f} Short: {1:.2f}".format(new_long_limit, new_short_limit))

            update_order_fields = UpdateOrderFields()
            update_order_fields.limit_price = new_long_limit
            update_order_fields.tag = "Update #{0}".format(len(long_order.update_requests) + 1)
            long_order.update(update_order_fields)

            update_order_fields = UpdateOrderFields()
            update_order_fields.limit_price = new_short_limit
            update_order_fields.tag = "Update #{0}".format(len(short_order.update_requests) + 1)
            short_order.update(update_order_fields)


    def stop_market_orders(self):
        '''StopMarketOrders work in the opposite way that limit orders do.
        When placing a long trade, the stop price must be above current
        market price. In this way it's a 'stop loss' for a short trade.
        When placing a short trade, the stop price must be below current
        market price. In this way it's a 'stop loss' for a long trade.
        You can submit requests to update or cancel the StopMarketOrder at any time.
        The 'StopPrice' for an order can be retrieved from the ticket using the
        OrderTicket.get(OrderField) method, for example:
        Code:
            current_stop_price = order_ticket.get(OrderField.STOP_PRICE)'''
        if self.time_is(7, 12 + 4, 0):
            self.log("Submitting StopMarketOrder")

            # a long stop is triggered when the price rises above the value
            # so we'll set a long stop .25% above the current bar's close
            close = self.securities[self.spy.value].close
            new_ticket = self.stop_market_order(self.spy, 10, close * 1.0025)
            self.__open_stop_market_orders.append(new_ticket)

            # a short stop is triggered when the price falls below the value
            # so we'll set a short stop .25% below the current bar's close
            new_ticket = self.stop_market_order(self.spy, -10, close * .9975)
            self.__open_stop_market_orders.append(new_ticket)

        # when we submitted new stop market orders we placed them into this list,
        # so while there's two entries they're still open and need processing
        if len(self.__open_stop_market_orders) == 2:
            # check if either is filled and cancel the other
            long_order = self.__open_stop_market_orders[0]
            short_order = self.__open_stop_market_orders[1]
            if self.check_pair_orders_for_fills(long_order, short_order):
                self.__open_stop_market_orders = []
                return

            # if neither order has filled, bring in the stops by a penny
            new_long_stop = long_order.get(OrderField.STOP_PRICE) - 0.01
            new_short_stop = short_order.get(OrderField.STOP_PRICE) + 0.01
            self.log("Updating stops - Long: {0:.2f} Short: {1:.2f}".format(new_long_stop, new_short_stop))

            update_order_fields = UpdateOrderFields()
            update_order_fields.stop_price = new_long_stop
            update_order_fields.tag = "Update #{0}".format(len(long_order.update_requests) + 1)
            long_order.update(update_order_fields)

            update_order_fields = UpdateOrderFields()
            update_order_fields.stop_price = new_short_stop
            update_order_fields.tag = "Update #{0}".format(len(short_order.update_requests) + 1)
            short_order.update(update_order_fields)
            self.log("Updated price - Long: {0} Short: {1}".format(long_order.get(OrderField.STOP_PRICE), short_order.get(OrderField.STOP_PRICE)))


    def stop_limit_orders(self):
        '''StopLimitOrders work as a combined stop and limit order. First, the
        price must pass the stop price in the same way a StopMarketOrder works,
        but then we're also guaranteed a fill price at least as good as the
        limit price. This order type can be beneficial in gap down scenarios
        where a StopMarketOrder would have triggered and given the not as beneficial
        gapped down price, whereas the StopLimitOrder could protect you from
        getting the gapped down price through prudent placement of the limit price.
        You can submit requests to update or cancel the StopLimitOrder at any time.
        The 'StopPrice' or 'LimitPrice' for an order can be retrieved from the ticket
        using the OrderTicket.get(OrderField) method, for example:
        Code:
            current_stop_price = order_ticket.get(OrderField.STOP_PRICE)
            current_limit_price = order_ticket.get(OrderField.LIMIT_PRICE)'''
        if self.time_is(8, 12, 1):
            self.log("Submitting StopLimitOrder")

            # a long stop is triggered when the price rises above the
            # value so we'll set a long stop .25% above the current bar's
            # close now we'll also be setting a limit, this means we are
            # guaranteed to get at least the limit price for our fills,
            # so make the limit price a little higher than the stop price

            close = self.securities[self.spy.value].close
            new_ticket = self.stop_limit_order(self.spy, 10, close * 1.001, close - 0.03)
            self.__open_stop_limit_orders.append(new_ticket)

            # a short stop is triggered when the price falls below the
            # value so we'll set a short stop .25% below the current bar's
            # close now we'll also be setting a limit, this means we are
            # guaranteed to get at least the limit price for our fills,
            # so make the limit price a little softer than the stop price

            new_ticket = self.stop_limit_order(self.spy, -10, close * .999, close + 0.03)
            self.__open_stop_limit_orders.append(new_ticket)

        # when we submitted new stop limit orders we placed them into this list,
        # so while there's two entries they're still open and need processing
        if len(self.__open_stop_limit_orders) == 2:
            long_order = self.__open_stop_limit_orders[0]
            short_order = self.__open_stop_limit_orders[1]
            if self.check_pair_orders_for_fills(long_order, short_order):
                self.__open_stop_limit_orders = []
                return

            # if neither order has filled, bring in the stops/limits in by a penny

            new_long_stop = long_order.get(OrderField.STOP_PRICE) - 0.01
            new_long_limit = long_order.get(OrderField.LIMIT_PRICE) + 0.01
            new_short_stop = short_order.get(OrderField.STOP_PRICE) + 0.01
            new_short_limit = short_order.get(OrderField.LIMIT_PRICE) - 0.01
            self.log("Updating stops  - Long: {0:.2f} Short: {1:.2f}".format(new_long_stop, new_short_stop))
            self.log("Updating limits - Long: {0:.2f}  Short: {1:.2f}".format(new_long_limit, new_short_limit))

            update_order_fields = UpdateOrderFields()
            update_order_fields.stop_price = new_long_stop
            update_order_fields.limit_price = new_long_limit
            update_order_fields.tag = "Update #{0}".format(len(long_order.update_requests) + 1)
            long_order.update(update_order_fields)

            update_order_fields = UpdateOrderFields()
            update_order_fields.stop_price = new_short_stop
            update_order_fields.limit_price = new_short_limit
            update_order_fields.tag = "Update #{0}".format(len(short_order.update_requests) + 1)
            short_order.update(update_order_fields)


    def trailing_stop_orders(self):
        '''TrailingStopOrders work the same way as StopMarketOrders, except
        their stop price is adjusted to a certain amount, keeping it a certain
        fixed distance from/to the market price, depending on the order direction,
        which allows to preserve profits and protecting against losses.
        The stop price can be accessed just as with StopMarketOrders, and
        the trailing amount can be accessed with the OrderTicket.get(OrderField), for example:
        Code:
            current_trailing_amount = order_ticket.get(OrderField.STOP_PRICE)
            trailing_as_percentage = order_ticket.get[bool](OrderField.TRAILING_AS_PERCENTAGE)'''
        if self.time_is(7, 12, 0):
            self.log("Submitting TrailingStopOrder")

            # a long stop is triggered when the price rises above the
            # value so we'll set a long stop .25% above the current bar's

            close = self.securities[self.spy.value].close
            stop_price = close * 1.0025
            new_ticket = self.trailing_stop_order(self.spy, 10, stop_price, trailing_amount=0.0025, trailing_as_percentage=True)
            self.__open_trailing_stop_orders.append(new_ticket)

            # a short stop is triggered when the price falls below the
            # value so we'll set a short stop .25% below the current bar's

            stop_price = close * .9975
            new_ticket = self.trailing_stop_order(self.spy, -10, stop_price, trailing_amount=0.0025, trailing_as_percentage=True)
            self.__open_trailing_stop_orders.append(new_ticket)

        # when we submitted new stop market orders we placed them into this list,
        # so while there's two entries they're still open and need processing
        elif len(self.__open_trailing_stop_orders) == 2:
            long_order = self.__open_trailing_stop_orders[0]
            short_order = self.__open_trailing_stop_orders[1]
            if self.check_pair_orders_for_fills(long_order, short_order):
                self.__open_trailing_stop_orders = []
                return

            # if neither order has filled in the last 5 minutes, bring in the trailing percentage by 0.01%
            if ((self.utc_time - long_order.time).total_seconds() / 60) % 5 != 0:
                return

            long_trailing_percentage = long_order.get(OrderField.TRAILING_AMOUNT)
            new_long_trailing_percentage = max(long_trailing_percentage - 0.0001, 0.0001)
            short_trailing_percentage = short_order.get(OrderField.TRAILING_AMOUNT)
            new_short_trailing_percentage = max(short_trailing_percentage - 0.0001, 0.0001)
            self.log("Updating trailing percentages - Long: {0:.3f} Short: {1:.3f}".format(new_long_trailing_percentage, new_short_trailing_percentage))

            update_order_fields = UpdateOrderFields()
            # we could change the quantity, but need to specify it
            #Quantity =
            update_order_fields.trailing_amount = new_long_trailing_percentage
            update_order_fields.tag = "Update #{0}".format(len(long_order.update_requests) + 1)
            long_order.update(update_order_fields)

            update_order_fields = UpdateOrderFields()
            update_order_fields.trailing_amount = new_short_trailing_percentage
            update_order_fields.tag = "Update #{0}".format(len(short_order.update_requests) + 1)
            short_order.update(update_order_fields)


    def market_on_close_orders(self):
        '''MarketOnCloseOrders are always executed at the next market's closing price.
        The only properties that can be updated are the quantity and order tag properties.'''
        if self.time_is(9, 12, 0):
            self.log("Submitting MarketOnCloseOrder")

            # open a new position or triple our existing position
            qty = self.portfolio[self.spy.value].quantity
            qty = 100 if qty == 0 else 2*qty

            new_ticket = self.market_on_close_order(self.spy, qty)
            self.__open_market_on_close_orders.append(new_ticket)

        if len(self.__open_market_on_close_orders) == 1 and self.time.minute == 59:
            ticket = self.__open_market_on_close_orders[0]
            # check for fills
            if ticket.status == OrderStatus.FILLED:
                self.__open_market_on_close_orders = []
                return

            quantity = ticket.quantity + 1
            self.log("Updating quantity  - New Quantity: {0}".format(quantity))

            # we can update the quantity and tag
            update_order_fields = UpdateOrderFields()
            update_order_fields.quantity = quantity
            update_order_fields.tag = "Update #{0}".format(len(ticket.update_requests) + 1)
            ticket.update(update_order_fields)

        if self.time_is(self.end_date.day, 12 + 3, 45):
            self.log("Submitting MarketOnCloseOrder to liquidate end of algorithm")
            self.market_on_close_order(self.spy, -self.portfolio[self.spy.value].quantity, "Liquidate end of algorithm")


    def market_on_open_orders(self):
        '''MarketOnOpenOrders are always executed at the next
        market's opening price. The only properties that can
        be updated are the quantity and order tag properties.'''
        if self.time_is(8, 12 + 2, 0):
            self.log("Submitting MarketOnOpenOrder")

            # its EOD, let's submit a market on open order to short even more!
            new_ticket = self.market_on_open_order(self.spy, 50)
            self.__open_market_on_open_orders.append(new_ticket)

        if len(self.__open_market_on_open_orders) == 1 and self.time.minute == 59:
            ticket = self.__open_market_on_open_orders[0]

            # check for fills
            if ticket.status == OrderStatus.FILLED:
                self.__open_market_on_open_orders = []
                return

            quantity = ticket.quantity + 1
            self.log("Updating quantity  - New Quantity: {0}".format(quantity))

            # we can update the quantity and tag
            update_order_fields = UpdateOrderFields()
            update_order_fields.quantity = quantity
            update_order_fields.tag = "Update #{0}".format(len(ticket.update_requests) + 1)
            ticket.update(update_order_fields)


    def on_order_event(self, order_event):
        order = self.transactions.get_order_by_id(order_event.order_id)
        self.log("{0}: {1}: {2}".format(self.time, order.type, order_event))

        if order_event.quantity == 0:
            raise Exception("OrderEvent quantity is Not expected to be 0, it should hold the current order Quantity")

        if order_event.quantity != order.quantity:
            raise Exception("OrderEvent quantity should hold the current order Quantity")

        if (type(order) is LimitOrder and order_event.limit_price == 0 or
            type(order) is StopLimitOrder and order_event.limit_price == 0):
            raise Exception("OrderEvent LimitPrice is Not expected to be 0 for LimitOrder and StopLimitOrder")

        if type(order) is StopMarketOrder and order_event.stop_price == 0:
            raise Exception("OrderEvent StopPrice is Not expected to be 0 for StopMarketOrder")

        # We can access the order ticket from the order event
        if order_event.ticket is None:
            raise Exception("OrderEvent Ticket was not set")
        if order_event.order_id != order_event.ticket.order_id:
            raise Exception("OrderEvent.ORDER_ID and order_event.ticket.order_id do not match")

    def check_pair_orders_for_fills(self, long_order, short_order):
        if long_order.status == OrderStatus.FILLED:
            self.log("{0}: Cancelling short order, long order is filled.".format(short_order.order_type))
            short_order.cancel("Long filled.")
            return True

        if short_order.status == OrderStatus.FILLED:
            self.log("{0}: Cancelling long order, short order is filled.".format(long_order.order_type))
            long_order.cancel("Short filled")
            return True

        return False


    def time_is(self, day, hour, minute):
        return self.time.day == day and self.time.hour == hour and self.time.minute == minute

    def on_end_of_algorithm(self):
        basic_order_ticket_filter = lambda x: x.symbol == self.spy

        filled_orders = self.transactions.get_orders(lambda x: x.status == OrderStatus.FILLED)
        order_tickets = self.transactions.get_order_tickets(basic_order_ticket_filter)
        open_orders = self.transactions.get_open_orders(lambda x: x.symbol == self.spy)
        open_order_tickets = self.transactions.get_open_order_tickets(basic_order_ticket_filter)
        remaining_open_orders = self.transactions.get_open_orders_remaining_quantity(basic_order_ticket_filter)

        # The type returned by self.transactions.get_orders() is iterable and not a list
        # that's why we use sum() to get the size of the iterable object type
        filled_orders_size = sum(1 for order in filled_orders)
        order_tickets_size = sum(1 for ticket in order_tickets)
        open_order_tickets_size = sum(1 for ticket in open_order_tickets)

        assert(filled_orders_size == 9 and order_tickets_size == 12), "There were expected 9 filled orders and 12 order tickets"
        assert(not (len(open_orders) or open_order_tickets_size)), "No open orders or tickets were expected"
        assert(not remaining_open_orders), "No remaining quantity to be filled from open orders was expected"

        spy_open_orders = self.transactions.get_open_orders(self.spy)
        spy_open_order_tickets = self.transactions.get_open_order_tickets(self.spy)
        spy_open_order_tickets_size = sum(1 for tickets in spy_open_order_tickets)
        spy_open_orders_remaining_quantity = self.transactions.get_open_orders_remaining_quantity(self.spy)

        assert(not (len(spy_open_orders) or spy_open_order_tickets_size)), "No open orders or tickets were expected"
        assert(not spy_open_orders_remaining_quantity), "No remaining quantity to be filled from open orders was expected"

        default_orders = self.transactions.get_orders()
        default_order_tickets = self.transactions.get_order_tickets()
        default_open_orders = self.transactions.get_open_orders()
        default_open_order_tickets = self.transactions.get_open_order_tickets()
        default_open_orders_remaining = self.transactions.get_open_orders_remaining_quantity()

        default_orders_size = sum(1 for order in default_orders)
        default_order_tickets_size = sum(1 for ticket in default_order_tickets)
        default_open_order_tickets_size = sum(1 for ticket in default_open_order_tickets)

        assert(default_orders_size == 12 and default_order_tickets_size == 12), "There were expected 12 orders and 12 order tickets"
        assert(not (len(default_open_orders) or default_open_order_tickets_size)), "No open orders or tickets were expected"
        assert(not default_open_orders_remaining), "No remaining quantity to be filled from open orders was expected"
