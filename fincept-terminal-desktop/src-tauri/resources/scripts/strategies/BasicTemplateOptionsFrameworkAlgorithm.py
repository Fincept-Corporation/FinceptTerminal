# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-778FF392
# Category: Options
# Description: Basic template options framework algorithm uses framework components to define an algorithm that trades options
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from Alphas.ConstantAlphaModel import ConstantAlphaModel
from Selection.OptionUniverseSelectionModel import OptionUniverseSelectionModel
from Execution.ImmediateExecutionModel import ImmediateExecutionModel
from Risk.NullRiskManagementModel import NullRiskManagementModel


### <summary>
### Basic template options framework algorithm uses framework components
### to define an algorithm that trades options.
### </summary>
class BasicTemplateOptionsFrameworkAlgorithm(QCAlgorithm):

    def initialize(self):
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2014, 6, 5)
        self.set_end_date(2014, 6, 9)
        self.set_cash(100000)

        # set framework models
        self.set_universe_selection(EarliestExpiringWeeklyAtTheMoneyPutOptionUniverseSelectionModel(self.select_option_chain_symbols))
        self.set_alpha(ConstantOptionContractAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(hours = 0.5)))
        self.set_portfolio_construction(SingleSharePortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(NullRiskManagementModel())


    def select_option_chain_symbols(self, utc_time):
        new_york_time = Extensions.convert_from_utc(utc_time, TimeZones.NEW_YORK)
        ticker = "TWX" if new_york_time.date() < date(2014, 6, 6) else "AAPL"
        return [ Symbol.create(ticker, SecurityType.OPTION, Market.USA, f"?{ticker}") ]

class EarliestExpiringWeeklyAtTheMoneyPutOptionUniverseSelectionModel(OptionUniverseSelectionModel):
    '''Creates option chain universes that select only the earliest expiry ATM weekly put contract
    and runs a user defined option_chain_symbol_selector every day to enable choosing different option chains'''
    def __init__(self, select_option_chain_symbols):
        super().__init__(timedelta(1), select_option_chain_symbols)

    def filter(self, filter):
        '''Defines the option chain universe filter'''
        return (filter.strikes(+1, +1)
                      # Expiration method accepts timedelta objects or integer for days.
                      # The following statements yield the same filtering criteria
                      .expiration(0, 7)
                      # .expiration(timedelta(0), timedelta(7))
                      .weeklys_only()
                      .puts_only()
                      .only_apply_filter_at_market_open())

class ConstantOptionContractAlphaModel(ConstantAlphaModel):
    '''Implementation of a constant alpha model that only emits insights for option symbols'''
    def __init__(self, type, direction, period):
        super().__init__(type, direction, period)

    def should_emit_insight(self, utc_time, symbol):
        # only emit alpha for option symbols and not underlying equity symbols
        if symbol.security_type != SecurityType.OPTION:
            return False

        return super().should_emit_insight(utc_time, symbol)

class SingleSharePortfolioConstructionModel(PortfolioConstructionModel):
    '''Portfolio construction model that sets target quantities to 1 for up insights and -1 for down insights'''
    def create_targets(self, algorithm, insights):
        targets = []
        for insight in insights:
            targets.append(PortfolioTarget(insight.symbol, insight.direction))
        return targets
