# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5C005A63
# Category: Execution Model
# Description: Regression algorithm to test ImmediateExecutionModel places orders with the correct quantity (taking into account the...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Execution.ImmediateExecutionModel import ImmediateExecutionModel
from QuantConnect.Orders import OrderEvent
# endregion

### <summary>
### Regression algorithm to test ImmediateExecutionModel places orders with the
### correct quantity (taking into account the fee's) so that the fill quantity
### is the expected one.
### </summary>
class ImmediateExecutionModelWorksWithBinanceFeeModel(QCAlgorithm):

    def Initialize(self):
        # *** initial configurations and backtest ***
        self.SetStartDate(2022, 12, 13)  # Set Start Date
        self.SetEndDate(2022, 12, 14)  # Set End Date
        self.SetAccountCurrency("BUSD") # Set Account Currency
        self.SetCash("BUSD", 100000, 1)  # Set Strategy Cash

        self.universe_settings.resolution = Resolution.MINUTE

        symbols = [ Symbol.create("BTCBUSD", SecurityType.CRYPTO, Market.BINANCE) ]

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, None))

        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(Resolution.MINUTE))
        self.set_execution(ImmediateExecutionModel())
        
        
        self.SetBrokerageModel(BrokerageName.Binance, AccountType.Margin)
    
    def on_order_event(self, order_event: OrderEvent) -> None:
        if order_event.status == OrderStatus.FILLED:
            if abs(order_event.quantity - 5.8) > 0.01:
                raise Exception(f"The expected quantity was 5.8 but the quantity from the order was {order_event.quantity}")
