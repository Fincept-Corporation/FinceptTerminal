# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-17047AD3
# Category: Regression Test
# Description: Regression algorithm asserting the behavior of auxiliary data history requests
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm asserting the behavior of auxiliary data history requests
### </summary>
class HistoryAuxiliaryDataRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2021, 1, 1)
        self.set_end_date(2021, 1, 5)

        aapl = self.add_equity("AAPL", Resolution.DAILY).symbol

        # multi symbol request
        spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)
        multi_symbol_request = self.history(Dividend, [ aapl, spy ], 360, Resolution.DAILY)
        if len(multi_symbol_request) != 12:
                raise ValueError(f"Unexpected multi symbol dividend count: {len(multi_symbol_request)}")

        # continuous future mapping requests
        sp500 = Symbol.create(Futures.Indices.SP_500_E_MINI, SecurityType.FUTURE, Market.CME)
        continuous_future_open_interest_mapping = self.history(SymbolChangedEvent, sp500, datetime(2007, 1, 1), datetime(2012, 1, 1), data_mapping_mode = DataMappingMode.OPEN_INTEREST)
        if len(continuous_future_open_interest_mapping) != 9:
                raise ValueError(f"Unexpected continuous future mapping event count: {len(continuous_future_open_interest_mapping)}")
        continuous_future_last_trading_day_mapping = self.history(SymbolChangedEvent, sp500, datetime(2007, 1, 1), datetime(2012, 1, 1), data_mapping_mode = DataMappingMode.LAST_TRADING_DAY)
        if len(continuous_future_last_trading_day_mapping) != 9:
                raise ValueError(f"Unexpected continuous future mapping event count: {len(continuous_future_last_trading_day_mapping)}")

        dividend = self.history(Dividend, aapl, 360)
        self.debug(str(dividend))
        if len(dividend) != 6:
            raise ValueError(f"Unexpected dividend count: {len(dividend)}")
        for distribution in dividend.distribution:
            if distribution == 0:
                raise ValueError(f"Unexpected distribution: {distribution}")

        split = self.history(Split, aapl, 360)
        self.debug(str(split))
        if len(split) != 2:
            raise ValueError(f"Unexpected split count: {len(split)}")
        for splitfactor in split.splitfactor:
            if splitfactor == 0:
                raise ValueError(f"Unexpected splitfactor: {splitfactor}")

        symbol = Symbol.create("BTCUSD", SecurityType.CRYPTO_FUTURE, Market.BINANCE)
        margin_interest = self.history(MarginInterestRate, symbol, 24 * 3, Resolution.HOUR)
        self.debug(str(margin_interest))
        if len(margin_interest) != 8:
            raise ValueError(f"Unexpected margin interest count: {len(margin_interest)}")
        for interestrate in margin_interest.interestrate:
            if interestrate == 0:
                raise ValueError(f"Unexpected interestrate: {interestrate}")

        # last trading date on 2007-05-18
        delisted_symbol = Symbol.create("AAA.1", SecurityType.EQUITY, Market.USA)
        delistings = self.history(Delisting, delisted_symbol, datetime(2007, 5, 15), datetime(2007, 5, 21))
        self.debug(str(delistings))
        if len(delistings) != 2:
            raise ValueError(f"Unexpected delistings count: {len(delistings)}")
        if delistings.iloc[0].type != DelistingType.WARNING:
            raise ValueError(f"Unexpected delisting: {delistings.iloc[0]}")
        if delistings.iloc[1].type != DelistingType.DELISTED:
            raise ValueError(f"Unexpected delisting: {delistings.iloc[1]}")

        # get's remapped:
        # 2008-09-30 spwr -> spwra
        # 2011-11-17 spwra -> spwr
        remapped_symbol = Symbol.create("SPWR", SecurityType.EQUITY, Market.USA)
        symbol_changed_events = self.history(SymbolChangedEvent, remapped_symbol, datetime(2007, 1, 1), datetime(2012, 1, 1))
        self.debug(str(symbol_changed_events))
        if len(symbol_changed_events) != 2:
            raise ValueError(f"Unexpected SymbolChangedEvents count: {len(symbol_changed_events)}")
        first_event = symbol_changed_events.iloc[0]
        if first_event.oldsymbol != "SPWR" or first_event.newsymbol != "SPWRA" or symbol_changed_events.index[0][1] != datetime(2008, 9, 30):
            raise ValueError(f"Unexpected SymbolChangedEvents: {first_event}")
        second_event = symbol_changed_events.iloc[1]
        if second_event.newsymbol != "SPWR" or second_event.oldsymbol != "SPWRA" or symbol_changed_events.index[1][1] != datetime(2011, 11, 17):
            raise ValueError(f"Unexpected SymbolChangedEvents: {second_event}")

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings("AAPL", 1)
