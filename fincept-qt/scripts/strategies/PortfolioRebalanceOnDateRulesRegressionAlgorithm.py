# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-53259B36
# Category: Portfolio Management
# Description: Regression algorithm testing portfolio construction model control over rebalancing, specifying a date rules, see GH 4075
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm testing portfolio construction model control over rebalancing,
### specifying a date rules, see GH 4075.
### </summary>
class PortfolioRebalanceOnDateRulesRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.universe_settings.resolution = Resolution.DAILY

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

        # let's use 0 minimum order margin percentage so we can assert trades are only submitted immediately after rebalance on Wednesday
        # if not, due to TPV variations happening every day we might no cross the minimum on wednesday but yes another day of the week
        self.settings.minimum_order_margin_portfolio_percentage = 0

        self.set_start_date(2015,1,1)
        self.set_end_date(2017,1,1)

        self.settings.rebalance_portfolio_on_insight_changes = False
        self.settings.rebalance_portfolio_on_security_changes = False

        self.set_universe_selection(CustomUniverseSelectionModel("CustomUniverseSelectionModel", lambda time: [ "AAPL", "IBM", "FB", "SPY" ]))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, TimeSpan.from_minutes(20), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(self.date_rules.every(DayOfWeek.WEDNESDAY)))
        self.set_execution(ImmediateExecutionModel())

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.SUBMITTED:
            self.debug(str(order_event))
            if self.utc_time.weekday() != 2:
                raise ValueError(str(self.utc_time) + " " + str(order_event.symbol) + " " + str(self.utc_time.weekday()))
