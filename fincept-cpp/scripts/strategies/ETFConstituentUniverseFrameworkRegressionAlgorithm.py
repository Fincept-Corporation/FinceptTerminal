# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D46C4922
# Category: ETF
# Description: Alpha model for ETF constituents, where we generate insights based on the weighting of the ETF constituent
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from typing import List
from AlgorithmImports import *

constituent_data = []

### <summary>
### Alpha model for ETF constituents, where we generate insights based on the weighting
### of the ETF constituent
### </summary>
class ETFConstituentAlphaModel(AlphaModel):
    def on_securities_changed(self, algorithm, changes):
        pass

    ### <summary>
    ### Creates new insights based on constituent data and their weighting
    ### in their respective ETF
    ### </summary>
    def update(self, algorithm: QCAlgorithm, data: Slice):
        insights = []

        for constituent in constituent_data:
            if constituent.symbol not in data.bars and \
                constituent.symbol not in data.quote_bars:

                continue

            insight_direction = InsightDirection.UP if constituent.weight is not None and constituent.weight >= 0.01 else InsightDirection.DOWN

            insights.append(Insight(
                algorithm.utc_time,
                constituent.symbol,
                timedelta(days=1),
                InsightType.PRICE,
                insight_direction,
                float(1 * int(insight_direction)),
                1.0,
                weight=float(0 if constituent.weight is None else constituent.weight)
            ))

        return insights

### <summary>
### Generates targets for ETF constituents, which will be set to the weighting
### of the constituent in their respective ETF
### </summary>
class ETFConstituentPortfolioModel(PortfolioConstructionModel):
    def __init__(self):
        self.has_added = False

    ### <summary>
    ### Securities changed, detects if we've got new additions to the universe
    ### so that we don't try to trade every loop
    ### </summary>
    def on_securities_changed(self, algorithm: QCAlgorithm, changes: SecurityChanges):
        self.has_added = len(changes.added_securities) != 0

    ### <summary>
    ### Creates portfolio targets based on the insights provided to us by the alpha model.
    ### Emits portfolio targets setting the quantity to the weight of the constituent
    ### in its respective ETF.
    ### </summary>
    def create_targets(self, algorithm: QCAlgorithm, insights: List[Insight]):
        if not self.has_added:
            return []

        final_insights = []
        for insight in insights:
            final_insights.append(PortfolioTarget(insight.symbol, float(0 if insight.weight is None else insight.weight)))
            self.has_added = False

        return final_insights

### <summary>
### Executes based on ETF constituent weighting
### </summary>
class ETFConstituentExecutionModel(ExecutionModel):
    ### <summary>
    ### Liquidates if constituents have been removed from the universe
    ### </summary>
    def on_securities_changed(self, algorithm: QCAlgorithm, changes: SecurityChanges):
        for change in changes.removed_securities:
            algorithm.liquidate(change.symbol)

    ### <summary>
    ### Creates orders for constituents that attempts to add
    ### the weighting of the constituent in our portfolio. The
    ### resulting algorithm portfolio weight might not be equal
    ### to the leverage of the ETF (1x, 2x, 3x, etc.)
    ### </summary>
    def execute(self, algorithm: QCAlgorithm, targets: List[IPortfolioTarget]):
        for target in targets:
            algorithm.set_holdings(target.symbol, target.quantity)

### <summary>
### Tests ETF constituents universe selection with the algorithm framework models (Alpha, PortfolioConstruction, Execution)
### </summary>
class ETFConstituentUniverseFrameworkRegressionAlgorithm(QCAlgorithm):
    ### <summary>
    ### Initializes the algorithm, setting up the framework classes and ETF constituent universe settings
    ### </summary>
    def initialize(self):
        self.set_start_date(2020, 12, 1)
        self.set_end_date(2021, 1, 31)
        self.set_cash(100000)

        self.set_alpha(ETFConstituentAlphaModel())
        self.set_portfolio_construction(ETFConstituentPortfolioModel())
        self.set_execution(ETFConstituentExecutionModel())

        spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)

        self.universe_settings.resolution = Resolution.HOUR
        universe = self.add_universe(self.universe.etf(spy, self.universe_settings, self.filter_etf_constituents))

        historical_data = self.history(universe, 1, flatten=True)
        if len(historical_data) < 200:
               raise ValueError(f"Unexpected universe DataCollection count {len(historical_data)}! Expected > 200")

    ### <summary>
    ### Filters ETF constituents
    ### </summary>
    ### <param name="constituents">ETF constituents</param>
    ### <returns>ETF constituent Symbols that we want to include in the algorithm</returns>
    def filter_etf_constituents(self, constituents):
        global constituent_data

        constituent_data_local = [i for i in constituents if i is not None and i.weight >= 0.001]
        constituent_data = list(constituent_data_local)

        return [i.symbol for i in constituent_data_local]

    ### <summary>
    ### no-op for performance
    ### </summary>
    def on_data(self, data):
        pass
