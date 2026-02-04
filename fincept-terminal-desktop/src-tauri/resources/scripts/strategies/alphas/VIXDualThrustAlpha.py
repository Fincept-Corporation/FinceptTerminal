# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-7DFAE1BB
# Category: Alpha Model
# Description: Alpha model that uses dual-thrust strategy to create insights
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

#
# This is a demonstration algorithm. It trades UVXY.
# Dual Thrust alpha model is used to produce insights.
# Those input parameters have been chosen that gave acceptable results on a series
# of random backtests run for the period from Oct, 2016 till Feb, 2019.
#

class VIXDualThrustAlpha(QCAlgorithm):

    def initialize(self):

        # -- STRATEGY INPUT PARAMETERS --
        self.k1 = 0.63
        self.k2 = 0.63
        self.range_period = 20
        self.consolidator_bars = 30

        # Settings
        self.set_start_date(2018, 10, 1)
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))
        self.set_brokerage_model(BrokerageName.INTERACTIVE_BROKERS_BROKERAGE, AccountType.MARGIN)

        # Universe Selection
        self.universe_settings.resolution = Resolution.MINUTE   # it's minute by default, but lets leave this param here
        symbols = [Symbol.create("SPY", SecurityType.EQUITY, Market.USA)]
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))

        # Warming up
        resolution_in_time_span =  Extensions.to_time_span(self.universe_settings.resolution)
        warm_up_time_span = Time.multiply(resolution_in_time_span, self.consolidator_bars)
        self.set_warm_up(warm_up_time_span)

        # Alpha Model
        self.set_alpha(DualThrustAlphaModel(self.k1, self.k2, self.range_period, self.universe_settings.resolution, self.consolidator_bars))

        ## Portfolio Construction
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        ## Execution
        self.set_execution(ImmediateExecutionModel())

        ## Risk Management
        self.set_risk_management(MaximumDrawdownPercentPerSecurity(0.03))


class DualThrustAlphaModel(AlphaModel):
    '''Alpha model that uses dual-thrust strategy to create insights
    https://medium.com/@FMZ_Quant/dual-thrust-trading-strategy-2cc74101a626
    or here:
    https://www.quantconnect.com/tutorials/strategy-library/dual-thrust-trading-algorithm'''

    def __init__(self,
                 k1,
                 k2,
                 range_period,
                 resolution = Resolution.DAILY,
                 bars_to_consolidate = 1):
        '''Initializes a new instance of the class
        Args:
            k1: Coefficient for upper band
            k2: Coefficient for lower band
            range_period: Amount of last bars to calculate the range
            resolution: The resolution of data sent into the EMA indicators
            bars_to_consolidate: If we want alpha to work on trade bars whose length is different
                from the standard resolution - 1m 1h etc. - we need to pass this parameters along
                with proper data resolution'''

        # coefficient that used to determine upper and lower borders of a breakout channel
        self.k1 = k1
        self.k2 = k2

        # period the range is calculated over
        self.range_period = range_period

        # initialize with empty dict.
        self._symbol_data_by_symbol = dict()

        # time for bars we make the calculations on
        resolution_in_time_span =  Extensions.to_time_span(resolution)
        self.consolidator_time_span = Time.multiply(resolution_in_time_span, bars_to_consolidate)

        # in 5 days after emission an insight is to be considered expired
        self.period = timedelta(5)

    def update(self, algorithm, data):
        insights = []

        for symbol, symbol_data in self._symbol_data_by_symbol.items():
            if not symbol_data.is_ready:
                continue

            holding = algorithm.portfolio[symbol]
            price = algorithm.securities[symbol].price

            # buying condition
            # - (1) price is above upper line
            # - (2) and we are not long. this is a first time we crossed the line lately
            if price > symbol_data.upper_line and not holding.is_long:
                insight_close_time_utc = algorithm.utc_time + self.period
                insights.append(Insight.price(symbol, insight_close_time_utc, InsightDirection.UP))

            # selling condition
            # - (1) price is lower that lower line
            # - (2) and we are not short. this is a first time we crossed the line lately
            if price < symbol_data.lower_line and not holding.is_short:
                insight_close_time_utc = algorithm.utc_time + self.period
                insights.append(Insight.price(symbol, insight_close_time_utc, InsightDirection.DOWN))

        return insights

    def on_securities_changed(self, algorithm, changes):
        # added
        for symbol in [x.symbol for x in changes.added_securities]:
            if symbol not in self._symbol_data_by_symbol:
                # add symbol/symbol_data pair to collection
                symbol_data = self.SymbolData(symbol, self.k1, self.k2, self.range_period, self.consolidator_time_span)
                self._symbol_data_by_symbol[symbol] = symbol_data
                # register consolidator
                algorithm.subscription_manager.add_consolidator(symbol, symbol_data.get_consolidator())

        # removed
        for symbol in [x.symbol for x in changes.removed_securities]:
            symbol_data = self._symbol_data_by_symbol.pop(symbol, None)
            if symbol_data is None:
                algorithm.error("Unable to remove data from collection: DualThrustAlphaModel")
            else:
                # unsubscribe consolidator from data updates
                algorithm.subscription_manager.remove_consolidator(symbol, symbol_data.get_consolidator())


    class SymbolData:
        '''Contains data specific to a symbol required by this model'''
        def __init__(self, symbol, k1, k2, range_period, consolidator_resolution):

            self.symbol = symbol
            self.range_window = RollingWindow[TradeBar](range_period)
            self.consolidator = TradeBarConsolidator(consolidator_resolution)

            def on_data_consolidated(sender, consolidated):
                # add new tradebar to
                self.range_window.add(consolidated)

                if self.range_window.is_ready:
                    hh = max([x.high for x in self.range_window])
                    hc = max([x.close for x in self.range_window])
                    lc = min([x.close for x in self.range_window])
                    ll = min([x.low for x in self.range_window])

                    range = max([hh - lc, hc - ll])
                    self.upper_line = consolidated.close + k1 * range
                    self.lower_line = consolidated.close - k2 * range

            # event fired at new consolidated trade bar
            self.consolidator.data_consolidated += on_data_consolidated

        # Returns the interior consolidator
        def get_consolidator(self):
            return self.consolidator

        @property
        def is_ready(self):
            return self.range_window.is_ready
