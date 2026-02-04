# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-6689DBF5
# Category: Alpha Model
# Description: Framework algorithm that uses the PearsonCorrelationPairsTradingAlphaModel. This model extendes BasePairsTradingAlpha...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Alphas.PearsonCorrelationPairsTradingAlphaModel import PearsonCorrelationPairsTradingAlphaModel

### <summary>
### Framework algorithm that uses the PearsonCorrelationPairsTradingAlphaModel.
### This model extendes BasePairsTradingAlphaModel and uses Pearson correlation
### to rank the pairs trading candidates and use the best candidate to trade.
### </summary>
class PearsonCorrelationPairsTradingAlphaModelFrameworkAlgorithm(QCAlgorithm):
    '''Framework algorithm that uses the PearsonCorrelationPairsTradingAlphaModel.
    This model extendes BasePairsTradingAlphaModel and uses Pearson correlation
    to rank the pairs trading candidates and use the best candidate to trade.'''

    def initialize(self):

        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)

        symbols = [Symbol.create(ticker, SecurityType.EQUITY, Market.USA)
            for ticker in ["SPY", "AIG", "BAC", "IBM"]]

        # Manually add SPY and AIG when the algorithm starts
        self.set_universe_selection(ManualUniverseSelectionModel(symbols[:2]))

        # At midnight, add all securities every day except on the last data
        # With this procedure, the Alpha Model will experience multiple universe changes
        self.add_universe_selection(ScheduledUniverseSelectionModel(
            self.date_rules.every_day(), self.time_rules.midnight,
            lambda dt: symbols if dt.day <= (self.end_date - timedelta(1)).day else []))

        self.set_alpha(PearsonCorrelationPairsTradingAlphaModel(252, Resolution.DAILY))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())

    def on_end_of_algorithm(self) -> None:
        # We have removed all securities from the universe. The Alpha Model should remove the consolidator
        consolidator_count = sum(s.consolidators.count for s in self.subscription_manager.subscriptions)
        if consolidator_count > 0:
            raise Exception(f"The number of consolidator should be zero. Actual: {consolidator_count}")
