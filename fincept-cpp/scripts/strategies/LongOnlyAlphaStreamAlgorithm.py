# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-BEFEB617
# Category: Alpha Model
# Description: Basic template framework algorithm uses framework components to define the algorithm. Shows EqualWeightingPortfolioCo...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel
from Execution.ImmediateExecutionModel import ImmediateExecutionModel
from Selection.ManualUniverseSelectionModel import ManualUniverseSelectionModel

### <summary>
### Basic template framework algorithm uses framework components to define the algorithm.
### Shows EqualWeightingPortfolioConstructionModel.long_only() application
### </summary>
### <meta name="tag" content="alpha streams" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="algorithm framework" />
class LongOnlyAlphaStreamAlgorithm(QCAlgorithm):

    def initialize(self):
        # 1. Required:
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

        # 2. Required: Alpha Streams Models:
        self.set_brokerage_model(BrokerageName.ALPHA_STREAMS)

        # 3. Required: Significant AUM Capacity
        self.set_cash(1000000)

        # Only SPY will be traded
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(Resolution.DAILY, PortfolioBias.LONG))
        self.set_execution(ImmediateExecutionModel())

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

        # Set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel(
            [Symbol.create(x, SecurityType.EQUITY, Market.USA) for x in ["SPY", "IBM"]]))

    def on_data(self, slice):
        if self.portfolio.invested: return

        self.emit_insights(
            [
                Insight.price("SPY", timedelta(1), InsightDirection.UP),
                Insight.price("IBM", timedelta(1), InsightDirection.DOWN)
            ])

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            if self.securities[order_event.symbol].holdings.is_short:
                raise ValueError("Invalid position, should not be short")
            self.debug(order_event)
