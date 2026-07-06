# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-B4A28E80
# Category: Portfolio Management
# Description: Regression algorithm testing portfolio construction model control over rebalancing, specifying a custom rebalance fun...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm testing portfolio construction model control over rebalancing,
### specifying a custom rebalance function that returns null in some cases, see GH 4075.
### </summary>
class PortfolioRebalanceOnCustomFuncRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.universe_settings.resolution = Resolution.DAILY

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

        self.set_start_date(2015, 1, 1)
        self.set_end_date(2018, 1, 1)

        self.settings.rebalance_portfolio_on_insight_changes = False
        self.settings.rebalance_portfolio_on_security_changes = False

        self.set_universe_selection(CustomUniverseSelectionModel("CustomUniverseSelectionModel", lambda time: [ "AAPL", "IBM", "FB", "SPY", "AIG", "BAC", "BNO" ]))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, TimeSpan.from_minutes(20), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(self.rebalance_function))
        self.set_execution(ImmediateExecutionModel())
        self.last_rebalance_time = self.start_date

    def rebalance_function(self, time):
        # for performance only run rebalance logic once a week, monday
        if time.weekday() != 0:
            return None

        if self.last_rebalance_time == self.start_date:
            # initial rebalance
            self.last_rebalance_time = time
            return time

        deviation = 0
        count = sum(1 for security in self.securities.values() if security.invested)
        if count > 0:
            self.last_rebalance_time = time
            portfolio_value_per_security = self.portfolio.total_portfolio_value / count
            for security in self.securities.values():
                if not security.invested:
                    continue
                reserved_buying_power_for_current_position = (security.buying_power_model.get_reserved_buying_power_for_position(
                    ReservedBuyingPowerForPositionParameters(security)).absolute_used_buying_power
                                                         * security.buying_power_model.get_leverage(security)) # see GH issue 4107
                # we sum up deviation for each security
                deviation += (portfolio_value_per_security - reserved_buying_power_for_current_position) / portfolio_value_per_security

            # if securities are deviated 1.5% from their theoretical share of TotalPortfolioValue we rebalance
            if deviation >= 0.015:
                return time
        return None

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.SUBMITTED:
            if self.utc_time != self.last_rebalance_time or self.utc_time.weekday() != 0:
                raise ValueError(f"{self.utc_time} {order_event.symbol}")
