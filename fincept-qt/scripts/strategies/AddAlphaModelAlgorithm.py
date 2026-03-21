# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-761339E9
# Category: Alpha Model
# Description: Test algorithm using 'QCAlgorithm.add_alpha_model()'
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Test algorithm using 'QCAlgorithm.add_alpha_model()'
### </summary>
class AddAlphaModelAlgorithm(QCAlgorithm):
    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self.universe_settings.resolution = Resolution.DAILY

        spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        fb = Symbol.create("FB", SecurityType.EQUITY, Market.USA)
        ibm = Symbol.create("IBM", SecurityType.EQUITY, Market.USA)

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel([ spy, fb, ibm ]))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

        self.add_alpha(OneTimeAlphaModel(spy))
        self.add_alpha(OneTimeAlphaModel(fb))
        self.add_alpha(OneTimeAlphaModel(ibm))

class OneTimeAlphaModel(AlphaModel):
    def __init__(self, symbol):
        self.symbol = symbol
        self.triggered = False

    def update(self, algorithm, data):
        insights = []
        if not self.triggered:
            self.triggered = True
            insights.append(Insight.price(
                self.symbol,
                Resolution.DAILY,
                1,
                InsightDirection.DOWN))
        return insights
