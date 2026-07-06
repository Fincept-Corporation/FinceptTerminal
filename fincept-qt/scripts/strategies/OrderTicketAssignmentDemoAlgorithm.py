# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D1943028
# Category: General Strategy
# Description: Demonstration on how to access order tickets right after placing an order
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from datetime import timedelta
from AlgorithmImports import *

### <summary>
### Demonstration on how to access order tickets right after placing an order.
### </summary>
class OrderTicketAssignmentDemoAlgorithm(QCAlgorithm):
    '''Demonstration on how to access order tickets right after placing an order.'''
    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)

        self._symbol = self.add_equity("SPY").symbol

        self.trade_count = 0
        self.consolidate(self._symbol, timedelta(hours=1), self.hour_consolidator)

    def hour_consolidator(self, bar: TradeBar):
        # Reset self.ticket to None on each new bar
        self.ticket = None
        self.ticket = self.market_order(self._symbol, 1, asynchronous=True)
        self.debug(f"{self.time}: Buy: Price {bar.price}, order_id: {self.ticket.order_id}")
        self.trade_count += 1

    def on_order_event(self, order_event: OrderEvent):
        # We cannot access self.ticket directly because it is assigned asynchronously:
        # this order event could be triggered before self.ticket is assigned.
        ticket = order_event.ticket
        if ticket is None:
            raise Exception("Expected order ticket in order event to not be null")
        if order_event.status == OrderStatus.SUBMITTED and self.ticket is not None:
            raise Exception("Field self.ticket not expected no be assigned on the first order event")

        self.debug(ticket.to_string())

    def on_end_of_algorithm(self):
        # Just checking that orders were placed
        if not self.portfolio.invested or self.trade_count != self.transactions.orders_count:
            raise Exception(f"Expected the portfolio to have holdings and to have {self.trade_count} trades, but had {self.transactions.orders_count}")
