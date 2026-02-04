# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3E91D425
# Category: Regression Test
# Description: Example algorithm showing that Slice, Securities and Portfolio behave as a Python Dictionary
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example algorithm showing that Slice, Securities and Portfolio behave as a Python Dictionary
### </summary>
class PythonDictionaryFeatureRegressionAlgorithm(QCAlgorithm):
    '''Example algorithm showing that Slice, Securities and Portfolio behave as a Python Dictionary'''

    def initialize(self):

        self.set_start_date(2013,10, 7)  #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self.spy_symbol = self.add_equity("SPY").symbol
        self.ibm_symbol = self.add_equity("IBM").symbol
        self.aig_symbol = self.add_equity("AIG").symbol
        self.aapl_symbol = Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)

        date_rules = self.date_rules.on(2013, 10, 7)
        self.schedule.on(date_rules, self.time_rules.at(13, 0), self.test_securities_dictionary)
        self.schedule.on(date_rules, self.time_rules.at(14, 0), self.test_portfolio_dictionary)
        self.schedule.on(date_rules, self.time_rules.at(15, 0), self.test_slice_dictionary)

    def test_slice_dictionary(self):
        slice = self.current_slice

        symbols = ', '.join([f'{x}' for x in slice.keys()])
        slice_data = ', '.join([f'{x}' for x in slice.values()])
        slice_bars = ', '.join([f'{x}' for x in slice.bars.values()])

        if "SPY" not in slice:
            raise Exception('SPY (string) is not in Slice')

        if self.spy_symbol not in slice:
            raise Exception('SPY (Symbol) is not in Slice')

        spy = slice.get(self.spy_symbol)
        if spy is None:
            raise Exception('SPY is not in Slice')

        for symbol, bar in slice.bars.items():
            self.plot(symbol, 'Price', bar.close)


    def test_securities_dictionary(self):
        symbols = ', '.join([f'{x}' for x in self.securities.keys()])
        leverages = ', '.join([str(x.get_last_data()) for x in self.securities.values()])

        if "IBM" not in self.securities:
            raise Exception('IBM (string) is not in Securities')

        if self.ibm_symbol not in self.securities:
            raise Exception('IBM (Symbol) is not in Securities')

        ibm = self.securities.get(self.ibm_symbol)
        if ibm is None:
            raise Exception('ibm is None')

        aapl = self.securities.get(self.aapl_symbol)
        if aapl is not None:
            raise Exception('aapl is not None')

        for symbol, security in self.securities.items():
            self.plot(symbol, 'Price', security.price)

    def test_portfolio_dictionary(self):
        symbols = ', '.join([f'{x}' for x in self.portfolio.keys()])
        leverages = ', '.join([f'{x.symbol}: {x.leverage}' for x in self.portfolio.values()])

        if "AIG" not in self.securities:
            raise Exception('AIG (string) is not in Portfolio')

        if self.aig_symbol not in self.securities:
            raise Exception('AIG (Symbol) is not in Portfolio')

        aig = self.portfolio.get(self.aig_symbol)
        if aig is None:
            raise Exception('aig is None')

        aapl = self.portfolio.get(self.aapl_symbol)
        if aapl is not None:
            raise Exception('aapl is not None')

        for symbol, holdings in self.portfolio.items():
            msg = f'{symbol}: {holdings.leverage}'

    def on_end_of_algorithm(self):

        portfolio_copy = self.portfolio.copy()
        try:
            self.portfolio.clear()  # Throws exception
        except Exception as e:
            self.debug(e)

        bar = self.securities.pop("SPY")
        length = len(self.securities)
        if length != 2:
            raise Exception(f'After popping SPY, Securities should have 2 elements, {length} found')

        securities_copy = self.securities.copy()
        self.securities.clear()    # Does not throw


    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1/3)
            self.set_holdings("IBM", 1/3)
            self.set_holdings("AIG", 1/3)
