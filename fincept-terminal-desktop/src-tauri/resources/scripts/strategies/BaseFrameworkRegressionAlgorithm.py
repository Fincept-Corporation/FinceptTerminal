# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0A7F740C
# Category: Regression Test
# Description: Abstract regression framework algorithm for multiple framework regression tests
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Abstract regression framework algorithm for multiple framework regression tests
### </summary>
class BaseFrameworkRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 6, 1)
        self.set_end_date(2014, 6, 30)

        self.universe_settings.resolution = Resolution.HOUR
        self.universe_settings.data_normalization_mode = DataNormalizationMode.RAW

        symbols = [Symbol.create(ticker, SecurityType.EQUITY, Market.USA)
            for ticker in ["AAPL", "AIG", "BAC", "SPY"]]

        # Manually add AAPL and AIG when the algorithm starts
        self.set_universe_selection(ManualUniverseSelectionModel(symbols[:2]))

        # At midnight, add all securities every day except on the last data
        # With this procedure, the Alpha Model will experience multiple universe changes
        self.add_universe_selection(ScheduledUniverseSelectionModel(
            self.date_rules.every_day(), self.time_rules.midnight,
            lambda dt: symbols if dt < self.end_date - timedelta(1) else []))

        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(31), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())

    def on_end_of_algorithm(self):
        # The base implementation checks for active insights
        insights_count = len(self.insights.get_insights(lambda insight: insight.is_active(self.utc_time)))
        if insights_count != 0:
            raise Exception(f"The number of active insights should be 0. Actual: {insights_count}")
