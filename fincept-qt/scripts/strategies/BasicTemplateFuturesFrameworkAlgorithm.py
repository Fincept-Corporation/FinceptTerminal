# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3F20A848
# Category: Futures
# Description: Basic template futures framework algorithm uses framework components to define an algorithm that trades futures
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from Alphas.ConstantAlphaModel import ConstantAlphaModel
from Selection.FutureUniverseSelectionModel import FutureUniverseSelectionModel

### <summary>
### Basic template futures framework algorithm uses framework components
### to define an algorithm that trades futures.
### </summary>
class BasicTemplateFuturesFrameworkAlgorithm(QCAlgorithm):

    def initialize(self):

        self.universe_settings.resolution = Resolution.MINUTE
        self.universe_settings.extended_market_hours = self.get_extended_market_hours()

        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)

        # set framework models
        self.set_universe_selection(FrontMonthFutureUniverseSelectionModel(self.select_future_chain_symbols))
        self.set_alpha(ConstantFutureContractAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1)))
        self.set_portfolio_construction(SingleSharePortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())


    def select_future_chain_symbols(self, utc_time):
        new_york_time = Extensions.convert_from_utc(utc_time, TimeZones.NEW_YORK)
        if new_york_time.date() < date(2013, 10, 9):
            return [ Symbol.create(Futures.Indices.SP_500_E_MINI, SecurityType.FUTURE, Market.CME) ]
        else:
            return [ Symbol.create(Futures.Metals.GOLD, SecurityType.FUTURE, Market.COMEX) ]

    def get_extended_market_hours(self):
        return False

class FrontMonthFutureUniverseSelectionModel(FutureUniverseSelectionModel):
    '''Creates futures chain universes that select the front month contract and runs a user
    defined future_chain_symbol_selector every day to enable choosing different futures chains'''
    def __init__(self, select_future_chain_symbols):
        super().__init__(timedelta(1), select_future_chain_symbols)

    def filter(self, filter):
        '''Defines the futures chain universe filter'''
        return (filter.front_month()
                      .only_apply_filter_at_market_open())

class ConstantFutureContractAlphaModel(ConstantAlphaModel):
    '''Implementation of a constant alpha model that only emits insights for future symbols'''
    def __init__(self, _type, direction, period):
        super().__init__(_type, direction, period)

    def should_emit_insight(self, utc_time, symbol):
        # only emit alpha for future symbols and not underlying equity symbols
        if symbol.security_type != SecurityType.FUTURE:
            return False

        return super().should_emit_insight(utc_time, symbol)

class SingleSharePortfolioConstructionModel(PortfolioConstructionModel):
    '''Portfolio construction model that sets target quantities to 1 for up insights and -1 for down insights'''
    def create_targets(self, algorithm, insights):
        targets = []
        for insight in insights:
            targets.append(PortfolioTarget(insight.symbol, insight.direction))
        return targets
