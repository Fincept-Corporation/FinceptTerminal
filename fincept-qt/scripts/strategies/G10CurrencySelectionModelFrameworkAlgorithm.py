# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-7CAB8ED5
# Category: General Strategy
# Description: Framework algorithm that uses the G10CurrencySelectionModel, a Universe Selection Model that inherits from ManualUniv...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Selection.ManualUniverseSelectionModel import ManualUniverseSelectionModel

class G10CurrencySelectionModel(ManualUniverseSelectionModel):
    '''Provides an implementation of IUniverseSelectionModel that simply subscribes to G10 currencies'''
    def __init__(self):
        '''Initializes a new instance of the G10CurrencySelectionModel class
        using the algorithm's security initializer and universe settings'''
        super().__init__([Symbol.create(x, SecurityType.FOREX, Market.OANDA)
                            for x in [ "EURUSD",
                                    "GBPUSD",
                                    "USDJPY",
                                    "AUDUSD",
                                    "NZDUSD",
                                    "USDCAD",
                                    "USDCHF",
                                    "USDNOK",
                                    "USDSEK" ]])

### <summary>
### Framework algorithm that uses the G10CurrencySelectionModel,
### a Universe Selection Model that inherits from ManualUniverseSelectionModel
### </summary>
class G10CurrencySelectionModelFrameworkAlgorithm(QCAlgorithm):
    '''Framework algorithm that uses the G10CurrencySelectionModel,
    a Universe Selection Model that inherits from ManualUniverseSelectionMode'''

    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        # set algorithm framework models
        self.set_universe_selection(G10CurrencySelectionModel())
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(MaximumDrawdownPercentPerSecurity(0.01))

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug("Purchased Stock: {0}".format(order_event.symbol))
