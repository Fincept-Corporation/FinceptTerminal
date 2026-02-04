# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D8FB9470
# Category: Regression Test
# Description: Regression algorithm to test we can liquidate our portfolio holdings using order properties
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm to test we can liquidate our portfolio holdings using order properties
### </summary>
class CanLiquidateWithOrderPropertiesRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2014, 6, 5)
        self.set_end_date(2014, 6, 6)
        self.set_cash(100000)
        
        self.open_exchange = datetime(2014, 6, 6, 10, 0, 0)
        self.close_exchange = datetime(2014, 6, 6, 16, 0, 0)
        self.add_equity("AAPL", resolution = Resolution.MINUTE)
    
    def on_data(self, slice):
        if self.time > self.open_exchange and self.time < self.close_exchange:
            if not self.portfolio.invested:
                self.market_order("AAPL", 10)
            else:
                order_properties = OrderProperties()
                order_properties.time_in_force = TimeInForce.DAY
                tickets = self.liquidate(asynchronous = True, order_properties = order_properties)
                for ticket in tickets:
                    if ticket.SubmitRequest.OrderProperties.TimeInForce != TimeInForce.DAY:
                        raise Exception(f"The TimeInForce for all orders should be daily, but it was {ticket.SubmitRequest.OrderProperties.TimeInForce}")
