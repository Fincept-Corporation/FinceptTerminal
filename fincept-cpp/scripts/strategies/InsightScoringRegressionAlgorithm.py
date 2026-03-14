# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E8E10658
# Category: Regression Test
# Description: Regression algorithm showing how to define a custom insight scoring function and using the insight manager
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm showing how to define a custom insight scoring function and using the insight manager
### </summary>
class InsightScoringRegressionAlgorithm(QCAlgorithm):
    '''Regression algorithm showing how to define a custom insight evaluator'''

    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2013,10,7)
        self.set_end_date(2013,10,11)
        
        symbols = [ Symbol.create("SPY", SecurityType.EQUITY, Market.USA) ]

        self.set_universe_selection(ManualUniverseSelectionModel(symbols))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(Resolution.DAILY))
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(MaximumDrawdownPercentPerSecurity(0.01))
        
        # we specify a custom insight evaluator
        self.insights.set_insight_score_function(CustomInsightScoreFunction(self.securities))

    def on_end_of_algorithm(self):
        all_insights = self.insights.get_insights(lambda insight: True)

        if len(all_insights) != 100 or len(self.insights.get_insights()) != 100:
            raise ValueError(f'Unexpected insight count found {all_insights.count}')

        if sum(1 for insight in all_insights if insight.score.magnitude == 0 or insight.score.direction == 0) < 5:
            raise ValueError(f'Insights not scored!')

        if sum(1 for insight in all_insights if insight.score.is_final_score) < 99:
            raise ValueError(f'Insights not finalized!')

class CustomInsightScoreFunction():

    def __init__(self, securities):
        self._securities = securities
        self._open_insights = {}

    def score(self, insight_manager, utc_time):
        open_insights = insight_manager.get_active_insights(utc_time)

        for insight in open_insights:
            self._open_insights[insight.id] = insight

        to_remove = []
        for open_insight in self._open_insights.values():
            security = self._securities[open_insight.symbol]
            open_insight.reference_value_final = security.price

            score = open_insight.reference_value_final - open_insight.reference_value
            open_insight.score.set_score(InsightScoreType.DIRECTION, score, utc_time)
            open_insight.score.set_score(InsightScoreType.MAGNITUDE, score * 2, utc_time)
            open_insight.estimated_value = score * 100

            if open_insight.is_expired(utc_time):
                open_insight.score.finalize(utc_time)
                to_remove.append(open_insight)

        # clean up
        for insight_to_remove in to_remove:
            self._open_insights.pop(insight_to_remove.id)
