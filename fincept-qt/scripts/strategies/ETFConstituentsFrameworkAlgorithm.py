# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A8E104E5
# Category: ETF
# Description: Demonstration of using the ETFConstituentsUniverseSelectionModel
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Selection.ETFConstituentsUniverseSelectionModel import *

### <summary>
### Demonstration of using the ETFConstituentsUniverseSelectionModel
### </summary>
class ETFConstituentsFrameworkAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 12, 1)
        self.set_end_date(2020, 12, 7)
        self.set_cash(100000)

        self.universe_settings.resolution = Resolution.DAILY
        symbol = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        self.add_universe_selection(ETFConstituentsUniverseSelectionModel(symbol, self.universe_settings, self.etf_constituents_filter))

        self.add_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(days=1)))

        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())


    def etf_constituents_filter(self, constituents: List[ETFConstituentData]) -> List[Symbol]:
        # Get the 10 securities with the largest weight in the index
        selected = sorted([c for c in constituents if c.weight],
            key=lambda c: c.weight, reverse=True)[:8]
        return [c.symbol for c in selected]

