# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C16FBA05
# Category: Historical Data
# Description: This algorithm demonstrates the various ways to handle History pandas DataFrame
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This algorithm demonstrates the various ways to handle History pandas DataFrame
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="history and warm up" />
### <meta name="tag" content="history" />
### <meta name="tag" content="warm up" />
class PandasDataFrameHistoryAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 6, 9)   # Set Start Date
        self.set_end_date(2014, 6, 9)     # Set End Date

        self.spy = self.add_equity("SPY", Resolution.DAILY).symbol
        self.eur = self.add_forex("EURUSD", Resolution.DAILY).symbol

        aapl = self.add_equity("AAPL", Resolution.MINUTE).symbol
        self.option = Symbol.create_option(aapl, Market.USA, OptionStyle.AMERICAN, OptionRight.CALL, 750, datetime(2014, 10, 18))
        self.add_option_contract(self.option)

        sp1 = self.add_data(QuandlFuture,"CHRIS/CME_SP1", Resolution.DAILY)
        sp1.exchange = EquityExchange()
        self.sp1 = sp1.symbol

        self.add_universe(self.coarse_selection)

    def coarse_selection(self, coarse):
        if self.portfolio.invested:
            return Universe.UNCHANGED

        selected = [x.symbol for x in coarse if x.symbol.value in ["AAA", "AIG", "BAC"]]
        if len(selected) == 0:
            return Universe.UNCHANGED

        universe_history = self.history(selected, 10, Resolution.DAILY)
        for symbol in selected:
            self.assert_history_index(universe_history, "close", 10, "", symbol)

        return selected


    def on_data(self, data):
        if self.portfolio.invested:
            return

        # we can get history in initialize to set up indicators and such
        self.spy_daily_sma = SimpleMovingAverage(14)

        # get the last calendar year's worth of SPY data at the configured resolution (daily)
        trade_bar_history = self.history(["SPY"], timedelta(365))
        self.assert_history_index(trade_bar_history, "close", 251, "SPY", self.spy)

        # get the last calendar year's worth of EURUSD data at the configured resolution (daily)
        quote_bar_history = self.history(["EURUSD"], timedelta(298))
        self.assert_history_index(quote_bar_history, "bidclose", 251, "EURUSD", self.eur)

        option_history = self.history([self.option], timedelta(3))
        option_history.index = option_history.index.droplevel(level=[0,1,2])
        self.assert_history_index(option_history, "bidclose", 390, "", self.option)

        # get the last calendar year's worth of quandl data at the configured resolution (daily)
        quandl_history = self.history(QuandlFuture, "CHRIS/CME_SP1", timedelta(365))
        self.assert_history_index(quandl_history, "settle", 251, "CHRIS/CME_SP1", self.sp1)

        # we can loop over the return value from these functions and we get TradeBars
        # we can use these TradeBars to initialize indicators or perform other math
        self.spy_daily_sma.reset()
        for index, trade_bar in trade_bar_history.loc["SPY"].iterrows():
            self.spy_daily_sma.update(index, trade_bar["close"])

        # we can loop over the return values from these functions and we'll get Quandl data
        # this can be used in much the same way as the trade_bar_history above
        self.spy_daily_sma.reset()
        for index, quandl in quandl_history.loc["CHRIS/CME_SP1"].iterrows():
            self.spy_daily_sma.update(index, quandl["settle"])

        self.set_holdings(self.eur, 1)

    def assert_history_index(self, df, column, expected, ticker, symbol):

        if df.empty:
            raise Exception(f"Empty history data frame for {symbol}")
        if column not in df:
            raise Exception(f"Could not unstack df. Columns: {', '.join(df.columns)} | {column}")

        value = df.iat[0,0]
        df2 = df.xs(df.index.get_level_values('time')[0], level='time')
        df3 = df[column].unstack(level=0)

        try:

            # str(Symbol.ID)
            self.assert_history_count(f"df.iloc[0]", df.iloc[0], len(df.columns))
            self.assert_history_count(f"df.loc[str({symbol.id})]", df.loc[str(symbol.id)], expected)
            self.assert_history_count(f"df.xs(str({symbol.id}))", df.xs(str(symbol.id)), expected)
            self.assert_history_count(f"df.at[(str({symbol.id}),), '{column}']", list(df.at[(str(symbol.id),), column]), expected)
            self.assert_history_count(f"df2.loc[str({symbol.id})]", df2.loc[str(symbol.id)], len(df2.columns))
            self.assert_history_count(f"df3[str({symbol.id})]", df3[str(symbol.id)], expected)
            self.assert_history_count(f"df3.get(str({symbol.id}))", df3.get(str(symbol.id)), expected)

            # str(Symbol)
            self.assert_history_count(f"df.loc[str({symbol})]", df.loc[str(symbol)], expected)
            self.assert_history_count(f"df.xs(str({symbol}))", df.xs(str(symbol)), expected)
            self.assert_history_count(f"df.at[(str({symbol}),), '{column}']", list(df.at[(str(symbol),), column]), expected)
            self.assert_history_count(f"df2.loc[str({symbol})]", df2.loc[str(symbol)], len(df2.columns))
            self.assert_history_count(f"df3[str({symbol})]", df3[str(symbol)], expected)
            self.assert_history_count(f"df3.get(str({symbol}))", df3.get(str(symbol)), expected)

            # str : Symbol.VALUE
            if len(ticker) == 0:
                return
            self.assert_history_count(f"df.loc[{ticker}]", df.loc[ticker], expected)
            self.assert_history_count(f"df.xs({ticker})", df.xs(ticker), expected)
            self.assert_history_count(f"df.at[(ticker,), '{column}']", list(df.at[(ticker,), column]), expected)
            self.assert_history_count(f"df2.loc[{ticker}]", df2.loc[ticker], len(df2.columns))
            self.assert_history_count(f"df3[{ticker}]", df3[ticker], expected)
            self.assert_history_count(f"df3.get({ticker})", df3.get(ticker), expected)

        except Exception as e:
            symbols = set(df.index.get_level_values(level='symbol'))
            raise Exception(f"{symbols}, {symbol.id}, {symbol}, {ticker}. {e}")


    def assert_history_count(self, method_call, trade_bar_history, expected):
        if isinstance(trade_bar_history, list):
            count = len(trade_bar_history)
        else:
            count = len(trade_bar_history.index)
        if count != expected:
            raise Exception(f"{method_call} expected {expected}, but received {count}")


class QuandlFuture(PythonQuandl):
    '''Custom quandl data type for setting customized value column name. Value column is used for the primary trading calculations and charting.'''
    def __init__(self):
        self.value_column_name = "Settle"
