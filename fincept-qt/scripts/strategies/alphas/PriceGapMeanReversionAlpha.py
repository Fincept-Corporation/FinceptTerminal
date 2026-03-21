# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-899805AE
# Category: Alpha Model
# Description: The motivating idea for this Alpha Model is that a large price gap (here we use true outliers --
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class PriceGapMeanReversionAlpha(QCAlgorithm):
    '''The motivating idea for this Alpha Model is that a large price gap (here we use true outliers --
    price gaps that whose absolutely values are greater than 3 * Volatility) is due to rebound
    back to an appropriate price or at least retreat from its brief extreme. Using a Coarse Universe selection
    function, the algorithm selects the top x-companies by Dollar Volume (x can be any number you choose)
    to trade with, and then uses the Standard Deviation of the 100 most-recent closing prices to determine
    which price movements are outliers that warrant emitting insights.

    This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open
    sourced so the community and client funds can see an example of an alpha.'''

    def initialize(self):

        self.set_start_date(2018, 1, 1)   #Set Start Date
        self.set_cash(100000)           #Set Strategy Cash

        ## Initialize variables to be used in controlling frequency of universe selection
        self.week = -1

        ## Manual Universe Selection
        self.universe_settings.resolution = Resolution.MINUTE
        self.set_universe_selection(CoarseFundamentalUniverseSelectionModel(self.coarse_selection_function))

        ## Set trading fees to $0
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))

        ## Set custom Alpha Model
        self.set_alpha(PriceGapMeanReversionAlphaModel())

        ## Set equal-weighting Portfolio Construction Model
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        ## Set Execution Model
        self.set_execution(ImmediateExecutionModel())

        ## Set Risk Management Model
        self.set_risk_management(NullRiskManagementModel())


    def coarse_selection_function(self, coarse):
        ## If it isn't a new week, return the same symbols
        current_week = self.time.isocalendar()[1]
        if current_week == self.week:
            return Universe.UNCHANGED
        self.week = current_week

        ## If its a new week, then re-filter stocks by Dollar Volume
        sorted_by_dollar_volume = sorted(coarse, key=lambda x: x.dollar_volume, reverse=True)

        return [ x.symbol for x in sorted_by_dollar_volume[:25] ]


class PriceGapMeanReversionAlphaModel:

    def __init__(self, *args, **kwargs):
        ''' Initialize variables and dictionary for Symbol Data to support algorithm's function '''
        self.lookback = 100
        self.resolution = kwargs['resolution'] if 'resolution' in kwargs else Resolution.MINUTE
        self.prediction_interval = Time.multiply(Extensions.to_time_span(self.resolution), 5) ## Arbitrary
        self._symbol_data_by_symbol = {}

    def update(self, algorithm, data):
        insights = []

        ## Loop through all Symbol Data objects
        for symbol, symbol_data in self._symbol_data_by_symbol.items():
            ## Evaluate whether or not the price jump is expected to rebound
            if not symbol_data.is_trend(data):
                continue

            ## Emit insights accordingly to the price jump sign
            direction = InsightDirection.DOWN if symbol_data.price_jump > 0 else InsightDirection.UP
            insights.append(Insight.price(symbol, self.prediction_interval, direction, symbol_data.price_jump, None))
            
        return insights

    def on_securities_changed(self, algorithm, changes):
        # Clean up data for removed securities
        for removed in changes.removed_securities:
            symbol_data = self._symbol_data_by_symbol.pop(removed.symbol, None)
            if symbol_data is not None:
                symbol_data.remove_consolidators(algorithm)

        symbols = [x.symbol for x in changes.added_securities
            if x.symbol not in self._symbol_data_by_symbol]

        history = algorithm.history(symbols, self.lookback, self.resolution)
        if history.empty: return

        ## Create and initialize SymbolData objects
        for symbol in symbols:
            symbol_data = SymbolData(algorithm, symbol, self.lookback, self.resolution)
            symbol_data.warm_up_indicators(history.loc[symbol])
            self._symbol_data_by_symbol[symbol] = symbol_data


class SymbolData:
    def __init__(self, algorithm, symbol, lookback, resolution):
        self._symbol = symbol
        self.close = 0
        self.last_price = 0
        self.price_jump = 0
        self.consolidator = algorithm.resolve_consolidator(symbol, resolution)
        self.volatility = StandardDeviation(f'{symbol}.std({lookback})', lookback)
        algorithm.register_indicator(symbol, self.volatility, self.consolidator)

    def remove_consolidators(self, algorithm):
        algorithm.subscription_manager.remove_consolidator(self._symbol, self.consolidator)

    def warm_up_indicators(self, history):
        self.close = history.iloc[-1].close
        for tuple in history.itertuples():
            self.volatility.update(tuple.Index, tuple.close)

    def is_trend(self, data):

        ## Check for any data events that would return a NoneBar in the Alpha Model Update() method
        if not data.bars.contains_key(self._symbol):
            return False

        self.last_price = self.close
        self.close = data.bars[self._symbol].close
        self.price_jump = (self.close / self.last_price) - 1
        return abs(100*self.price_jump) > 3*self.volatility.current.value
