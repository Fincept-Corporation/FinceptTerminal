# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-13992977
# Category: Brokerage
# Description: Example algorithm to demostrate the event handlers of Brokerage activities
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example algorithm to demostrate the event handlers of Brokerage activities
### </summary>
### <meta name="tag" content="using quantconnect" />
class BrokerageActivityEventHandlingAlgorithm(QCAlgorithm):
    
    ### <summary>
    ### Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    ### </summary>
    def initialize(self):
        self.set_start_date(2013, 10, 07)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)

        self.add_equity("SPY", Resolution.MINUTE)

    ### <summary>
    ### on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    ### </summary>
    ### <param name="data">Slice object keyed by symbol containing the stock data</param>
    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)

    ### <summary>
    ### Brokerage message event handler. This method is called for all types of brokerage messages.
    ### </summary>
    def on_brokerage_message(self, message_event):
        self.debug(f"Brokerage meesage received - {message_event.to_string()}")

    ### <summary>
    ### Brokerage disconnected event handler. This method is called when the brokerage connection is lost.
    ### </summary>
    def on_brokerage_disconnect(self):
        self.debug(f"Brokerage disconnected!")
    
    ### <summary>
    ### Brokerage reconnected event handler. This method is called when the brokerage connection is restored after a disconnection.
    ### </summary>
    def on_brokerage_reconnect(self):
        self.debug(f"Brokerage reconnected!")
