# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-46661FE6
# Category: Alpha Model
# Description: Alpha Streams: Benchmark Alpha: Pick stocks according to Joel Greenblatt's Magic Formula
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Selection.FundamentalUniverseSelectionModel import FundamentalUniverseSelectionModel

from math import ceil
from itertools import chain

class GreenblattMagicFormulaAlpha(QCAlgorithm):
    ''' Alpha Streams: Benchmark Alpha: Pick stocks according to Joel Greenblatt's Magic Formula
    This alpha picks stocks according to Joel Greenblatt's Magic Formula.
    First, each stock is ranked depending on the relative value of the ratio EV/EBITDA. For example, a stock
    that has the lowest EV/EBITDA ratio in the security universe receives a score of one while a stock that has
    the tenth lowest EV/EBITDA score would be assigned 10 points.

    Then, each stock is ranked and given a score for the second valuation ratio, Return on Capital (ROC).
    Similarly, a stock that has the highest ROC value in the universe gets one score point.
    The stocks that receive the lowest combined score are chosen for insights.

    Source: Greenblatt, J. (2010) The Little Book That Beats the Market

    This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open
    sourced so the community and client funds can see an example of an alpha.'''

    def initialize(self):

        self.set_start_date(2018, 1, 1)
        self.set_cash(100000)

        #Set zero transaction fees
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))

        # select stocks using MagicFormulaUniverseSelectionModel
        self.set_universe_selection(GreenBlattMagicFormulaUniverseSelectionModel())

        # Use MagicFormulaAlphaModel to establish insights
        self.set_alpha(RateOfChangeAlphaModel())

        # Equally weigh securities in portfolio, based on insights
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        ## Set Immediate Execution Model
        self.set_execution(ImmediateExecutionModel())

        ## Set Null Risk Management Model
        self.set_risk_management(NullRiskManagementModel())

class RateOfChangeAlphaModel(AlphaModel):
    '''Uses Rate of Change (ROC) to create magnitude prediction for insights.'''

    def __init__(self, *args, **kwargs):
        self.lookback = kwargs.get('lookback', 1)
        self.resolution = kwargs.get('resolution', Resolution.DAILY)
        self.prediction_interval = Time.multiply(Extensions.to_time_span(self.resolution), self.lookback)
        self._symbol_data_by_symbol = {}

    def update(self, algorithm, data):
        insights = []
        for symbol, symbol_data in self._symbol_data_by_symbol.items():
            if symbol_data.can_emit:
                insights.append(Insight.price(symbol, self.prediction_interval, InsightDirection.UP, symbol_data.returns, None))
        return insights

    def on_securities_changed(self, algorithm, changes):

        # clean up data for removed securities
        for removed in changes.removed_securities:
            symbol_data = self._symbol_data_by_symbol.pop(removed.symbol, None)
            if symbol_data is not None:
                symbol_data.remove_consolidators(algorithm)

        # initialize data for added securities
        symbols = [ x.symbol for x in changes.added_securities
            if x.symbol not in self._symbol_data_by_symbol]

        history = algorithm.history(symbols, self.lookback, self.resolution)
        if history.empty: return

        for symbol in symbols:
            symbol_data = SymbolData(algorithm, symbol, self.lookback, self.resolution)
            self._symbol_data_by_symbol[symbol] = symbol_data
            symbol_data.warm_up_indicators(history.loc[symbol])


class SymbolData:
    '''Contains data specific to a symbol required by this model'''
    def __init__(self, algorithm, symbol, lookback, resolution):
        self.previous = 0
        self._symbol = symbol
        self.roc = RateOfChange(f'{symbol}.roc({lookback})', lookback)
        self.consolidator = algorithm.resolve_consolidator(symbol, resolution)
        algorithm.register_indicator(symbol, self.roc, self.consolidator)

    def remove_consolidators(self, algorithm):
        algorithm.subscription_manager.remove_consolidator(self._symbol, self.consolidator)

    def warm_up_indicators(self, history):
        for tuple in history.itertuples():
            self.roc.update(tuple.Index, tuple.close)

    @property
    def returns(self):
        return self.roc.current.value

    @property
    def can_emit(self):
        if self.previous == self.roc.samples:
            return False

        self.previous = self.roc.samples
        return self.roc.is_ready

    def __str__(self, **kwargs):
        return f'{self.roc.name}: {(1 + self.returns)**252 - 1:.2%}'


class GreenBlattMagicFormulaUniverseSelectionModel(FundamentalUniverseSelectionModel):
    '''Defines a universe according to Joel Greenblatt's Magic Formula, as a universe selection model for the framework algorithm.
       From the universe QC500, stocks are ranked using the valuation ratios, Enterprise Value to EBITDA (EV/EBITDA) and Return on Assets (ROA).
    '''

    def __init__(self,
                 filter_fine_data = True,
                 universe_settings = None):
        '''Initializes a new default instance of the MagicFormulaUniverseSelectionModel'''
        super().__init__(filter_fine_data, universe_settings)

        # Number of stocks in Coarse Universe
        self.number_of_symbols_coarse = 500
        # Number of sorted stocks in the fine selection subset using the valuation ratio, EV to EBITDA (EV/EBITDA)
        self.number_of_symbols_fine = 20
        # Final number of stocks in security list, after sorted by the valuation ratio, Return on Assets (ROA)
        self.number_of_symbols_in_portfolio = 10

        self.last_month = -1
        self.dollar_volume_by_symbol = {}

    def select_coarse(self, algorithm, coarse):
        '''Performs coarse selection for constituents.
        The stocks must have fundamental data'''
        month = algorithm.time.month
        if month == self.last_month:
            return Universe.UNCHANGED
        self.last_month = month

        # sort the stocks by dollar volume and take the top 1000
        top = sorted([x for x in coarse if x.has_fundamental_data],
                    key=lambda x: x.dollar_volume, reverse=True)[:self.number_of_symbols_coarse]

        self.dollar_volume_by_symbol = { i.symbol: i.dollar_volume for i in top }

        return list(self.dollar_volume_by_symbol.keys())


    def select_fine(self, algorithm, fine):
        '''QC500: Performs fine selection for the coarse selection constituents
        The company's headquarter must in the U.S.
        The stock must be traded on either the NYSE or NASDAQ
        At least half a year since its initial public offering
        The stock's market cap must be greater than 500 million

        Magic Formula: Rank stocks by Enterprise Value to EBITDA (EV/EBITDA)
        Rank subset of previously ranked stocks (EV/EBITDA), using the valuation ratio Return on Assets (ROA)'''

        # QC500:
        ## The company's headquarter must in the U.S.
        ## The stock must be traded on either the NYSE or NASDAQ
        ## At least half a year since its initial public offering
        ## The stock's market cap must be greater than 500 million
        filtered_fine = [x for x in fine if x.company_reference.country_id == "USA"
                                        and (x.company_reference.primary_exchange_id == "NYS" or x.company_reference.primary_exchange_id == "NAS")
                                        and (algorithm.time - x.security_reference.ipo_date).days > 180
                                        and x.earning_reports.basic_average_shares.three_months * x.earning_reports.basic_eps.twelve_months * x.valuation_ratios.pe_ratio > 5e8]
        count = len(filtered_fine)
        if count == 0: return []

        my_dict = dict()
        percent = self.number_of_symbols_fine / count

        # select stocks with top dollar volume in every single sector
        for key in ["N", "M", "U", "T", "B", "I"]:
            value = [x for x in filtered_fine if x.company_reference.industry_template_code == key]
            value = sorted(value, key=lambda x: self.dollar_volume_by_symbol[x.symbol], reverse = True)
            my_dict[key] = value[:ceil(len(value) * percent)]

        # stocks in QC500 universe
        top_fine = chain.from_iterable(my_dict.values())

        #  Magic Formula:
        ## Rank stocks by Enterprise Value to EBITDA (EV/EBITDA)
        ## Rank subset of previously ranked stocks (EV/EBITDA), using the valuation ratio Return on Assets (ROA)

        # sort stocks in the security universe of QC500 based on Enterprise Value to EBITDA valuation ratio
        sorted_by_ev_to_ebitda = sorted(top_fine, key=lambda x: x.valuation_ratios.ev_to_ebitda , reverse=True)

        # sort subset of stocks that have been sorted by Enterprise Value to EBITDA, based on the valuation ratio Return on Assets (ROA)
        sorted_by_roa = sorted(sorted_by_ev_to_ebitda[:self.number_of_symbols_fine], key=lambda x: x.valuation_ratios.forward_roa, reverse=False)

        # retrieve list of securites in portfolio
        return [f.symbol for f in sorted_by_roa[:self.number_of_symbols_in_portfolio]]
