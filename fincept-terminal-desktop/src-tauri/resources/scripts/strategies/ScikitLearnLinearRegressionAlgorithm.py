# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1244BB8B
# Category: Regression Test
# Description: Scikit Learn Linear Regression Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from sklearn.linear_model import LinearRegression

class ScikitLearnLinearRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)  # Set Start Date
        self.set_end_date(2013, 10, 8) # Set End Date

        self.lookback = 30 # number of previous days for training

        self.set_cash(100000)  # Set Strategy Cash
        spy = self.add_equity("SPY", Resolution.MINUTE)

        self.symbols = [ spy.symbol ] # In the future, we can include more symbols to the list in this way

        self.schedule.on(self.date_rules.every_day("SPY"), self.time_rules.after_market_open("SPY", 28), self.regression)
        self.schedule.on(self.date_rules.every_day("SPY"), self.time_rules.after_market_open("SPY", 30), self.trade)


    def regression(self):
        # Daily historical data is used to train the machine learning model
        history = self.history(self.symbols, self.lookback, Resolution.DAILY)

        # price dictionary:    key: symbol; value: historical price
        self.prices = {}
        # slope dictionary:    key: symbol; value: slope
        self.slopes = {}

        for symbol in self.symbols:
            if not history.empty:
                # get historical open price
                self.prices[symbol] = list(history.loc[symbol.value]['open'])

        # A is the design matrix
        A = range(self.lookback + 1)

        for symbol in self.symbols:
            if symbol in self.prices:
                # response
                Y = self.prices[symbol]
                # features
                X = np.column_stack([np.ones(len(A)), A])

                # data preparation
                length = min(len(X), len(Y))
                X = X[-length:]
                Y = Y[-length:]
                A = A[-length:]

                # fit the linear regression
                reg = LinearRegression().fit(X, Y)

                # run linear regression y = ax + b
                b = reg.intercept_
                a = reg.coef_[1]

                # store slopes for symbols
                self.slopes[symbol] = a/b


    def trade(self):
        # if there is no open price
        if not self.prices:
            return

        thod_buy = 0.001 # threshold of slope to buy
        thod_liquidate = -0.001 # threshold of slope to liquidate

        for holding in self.portfolio.Values:
            slope = self.slopes[holding.symbol]
            # liquidate when slope smaller than thod_liquidate
            if holding.invested and slope < thod_liquidate:
                self.liquidate(holding.symbol)

        for symbol in self.symbols:
            # buy when slope larger than thod_buy
            if self.slopes[symbol] > thod_buy:
                self.set_holdings(symbol, 1 / len(self.symbols))
