# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-EB692DBC
# Category: General Strategy
# Description: Algorithm for testing submit/update/cancel for combo orders
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
import itertools
from AlgorithmImports import *

### <summary>
### Algorithm for testing submit/update/cancel for combo orders
### </summary>
class ComboOrderTicketDemoAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(100000)

        equity = self.add_equity("GOOG", leverage=4, fill_forward=True)
        option = self.add_option(equity.symbol, fill_forward=True)
        self._option_symbol = option.symbol

        option.set_filter(lambda u: u.strikes(-2, +2).expiration(0, 180))

        self._open_market_orders = []
        self._open_leg_limit_orders = []
        self._open_limit_orders = []

        self._order_legs = None

    def on_data(self, data: Slice):
        if self._order_legs is None:
            if self.is_market_open(self._option_symbol):
                chain = data.option_chains.get_value(self._option_symbol)
                if chain is not None:
                    call_contracts = [contract for contract in chain if contract.right == OptionRight.CALL]
                    call_contracts = [(key, list(group)) for key, group in itertools.groupby(call_contracts, key=lambda x: x.expiry)]
                    call_contracts.sort(key=lambda x: x[0])
                    call_contracts = call_contracts[0][1]
                    call_contracts.sort(key=lambda x: x.strike)

                    if len(call_contracts) < 3:
                        return

                    quantities = [1, -2, 1]
                    self._order_legs = []
                    for i, contract in enumerate(call_contracts[:3]):
                        leg = Leg.create(contract.symbol, quantities[i])
                        self._order_legs.append(leg)
        else:
            # COMBO MARKET ORDERS

            self.combo_market_orders()

            # COMBO LIMIT ORDERS

            self.combo_limit_orders()

            # COMBO LEG LIMIT ORDERS

            self.combo_leg_limit_orders()

    def combo_market_orders(self):
        if len(self._open_market_orders) != 0 or self._order_legs is None:
            return

        self.log("Submitting combo market orders")

        tickets = self.combo_market_order(self._order_legs, 2, asynchronous=False)
        self._open_market_orders.extend(tickets)

        tickets = self.combo_market_order(self._order_legs, 2, asynchronous=True)
        self._open_market_orders.extend(tickets)

        for ticket in tickets:
            response = ticket.cancel("Attempt to cancel combo market order")
            if response.is_success:
                raise Exception("Combo market orders should fill instantly, they should not be cancelable in backtest mode: " + response.order_id)

    def combo_limit_orders(self):
        if len(self._open_limit_orders) == 0:
            self.log("Submitting ComboLimitOrder")

            current_price = sum([leg.quantity * self.securities[leg.symbol].close for leg in self._order_legs])

            tickets = self.combo_limit_order(self._order_legs, 2, current_price + 1.5)
            self._open_limit_orders.extend(tickets)

            # These won't fill, we will test cancel with this
            tickets = self.combo_limit_order(self._order_legs, -2, current_price + 3)
            self._open_limit_orders.extend(tickets)
        else:
            combo1 = self._open_limit_orders[:len(self._order_legs)]
            combo2 = self._open_limit_orders[-len(self._order_legs):]

            # check if either is filled and cancel the other
            if self.check_group_orders_for_fills(combo1, combo2):
                return

            # if neither order has filled, bring in the limits by a penny

            ticket = combo1[0]
            new_limit = round(ticket.get(OrderField.LIMIT_PRICE) + 0.01, 2)
            self.debug(f"Updating limits - Combo 1 {ticket.order_id}: {new_limit:.2f}")
            fields = UpdateOrderFields()
            fields.limit_price = new_limit
            fields.tag = f"Update #{len(ticket.update_requests) + 1}"
            ticket.update(fields)

            ticket = combo2[0]
            new_limit = round(ticket.get(OrderField.LIMIT_PRICE) - 0.01, 2)
            self.debug(f"Updating limits - Combo 2 {ticket.order_id}: {new_limit:.2f}")
            fields.limit_price = new_limit
            fields.tag = f"Update #{len(ticket.update_requests) + 1}"
            ticket.update(fields)

    def combo_leg_limit_orders(self):
        if len(self._open_leg_limit_orders) == 0:
            self.log("Submitting ComboLegLimitOrder")

            # submit a limit order to buy 2 shares at .1% below the bar's close
            for leg in self._order_legs:
                close = self.securities[leg.symbol].close
                leg.order_price = close * .999

            tickets = self.combo_leg_limit_order(self._order_legs, quantity=2)
            self._open_leg_limit_orders.extend(tickets)

            # submit another limit order to sell 2 shares at .1% above the bar's close
            for leg in self._order_legs:
                close = self.securities[leg.symbol].close
                leg.order_price = close * 1.001

            tickets = self.combo_leg_limit_order(self._order_legs, -2)
            self._open_leg_limit_orders.extend(tickets)
        else:
            combo1 = self._open_leg_limit_orders[:len(self._order_legs)]
            combo2 = self._open_leg_limit_orders[-len(self._order_legs):]

            # check if either is filled and cancel the other
            if self.check_group_orders_for_fills(combo1, combo2):
                return

            # if neither order has filled, bring in the limits by a penny

            for ticket in combo1:
                new_limit = round(ticket.get(OrderField.LIMIT_PRICE) + (1 if ticket.quantity > 0 else -1) * 0.01, 2)
                self.debug(f"Updating limits - Combo #1: {new_limit:.2f}")
                fields = UpdateOrderFields()
                fields.limit_price = new_limit
                fields.tag = f"Update #{len(ticket.update_requests) + 1}"
                ticket.update(fields)

            for ticket in combo2:
                new_limit = round(ticket.get(OrderField.LIMIT_PRICE) + (1 if ticket.quantity > 0 else -1) * 0.01, 2)
                self.debug(f"Updating limits - Combo #2: {new_limit:.2f}")
                fields.limit_price = new_limit
                fields.tag = f"Update #{len(ticket.update_requests) + 1}"
                ticket.update(fields)

    def on_order_event(self, order_event):
        order = self.transactions.get_order_by_id(order_event.order_id)

        if order_event.quantity == 0:
            raise Exception("OrderEvent quantity is Not expected to be 0, it should hold the current order Quantity")

        if order_event.quantity != order.quantity:
            raise Exception("OrderEvent quantity should hold the current order Quantity. "
                            f"Got {order_event.quantity}, expected {order.quantity}")

        if order.type == OrderType.COMBO_LEG_LIMIT and order_event.limit_price == 0:
            raise Exception("OrderEvent.LIMIT_PRICE is not expected to be 0 for ComboLegLimitOrder")

    def check_group_orders_for_fills(self, combo1, combo2):
        if all(x.status == OrderStatus.FILLED for x in combo1):
            self.log(f"{combo1[0].order_type}: Canceling combo #2, combo #1 is filled.")
            if any(OrderExtensions.is_open(x.status) for x in combo2):
                for ticket in combo2:
                    ticket.cancel("Combo #1 filled.")
            return True

        if all(x.status == OrderStatus.FILLED for x in combo2):
            self.log(f"{combo2[0].order_type}: Canceling combo #1, combo #2 is filled.")
            if any(OrderExtensions.is_open(x.status) for x in combo1):
                for ticket in combo1:
                    ticket.cancel("Combo #2 filled.")
            return True

        return False

    def on_end_of_algorithm(self):
        filled_orders = self.transactions.get_orders(lambda x: x.status == OrderStatus.FILLED).to_list()
        order_tickets = self.transactions.get_order_tickets().to_list()
        open_orders = self.transactions.get_open_orders()
        open_order_tickets = self.transactions.get_open_order_tickets().to_list()
        remaining_open_orders = self.transactions.get_open_orders_remaining_quantity()

        # 6 market, 6 limit, 6 leg limit.
        # Out of the 6 limit orders, 3 are expected to be canceled.
        expected_orders_count = 18
        expected_fills_count = 15
        if len(filled_orders) != expected_fills_count or len(order_tickets) != expected_orders_count:
            raise Exception(f"There were expected {expected_fills_count} filled orders and {expected_orders_count} order tickets, but there were {len(filled_orders)} filled orders and {len(order_tickets)} order tickets")

        filled_combo_market_orders = [x for x in filled_orders if x.type == OrderType.COMBO_MARKET]
        filled_combo_limit_orders = [x for x in filled_orders if x.type == OrderType.COMBO_LIMIT]
        filled_combo_leg_limit_orders = [x for x in filled_orders if x.type == OrderType.COMBO_LEG_LIMIT]
        if len(filled_combo_market_orders) != 6 or len(filled_combo_limit_orders) != 3 or len(filled_combo_leg_limit_orders) != 6:
            raise Exception("There were expected 6 filled market orders, 3 filled combo limit orders and 6 filled combo leg limit orders, "
                            f"but there were {len(filled_combo_market_orders)} filled market orders, {len(filled_combo_limit_orders)} filled "
                            f"combo limit orders and {len(filled_combo_leg_limit_orders)} filled combo leg limit orders")

        if len(open_orders) != 0 or len(open_order_tickets) != 0:
            raise Exception("No open orders or tickets were expected")

        if remaining_open_orders != 0:
            raise Exception("No remaining quantity to be filled from open orders was expected")
