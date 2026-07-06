# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-8863BE2D
# Category: Machine Learning
# Description: Save the data and the mode using the ObjectStore
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from keras.models import *
from tensorflow import keras
from keras.layers import Dense, Activation
from keras.optimizers import SGD

class KerasNeuralNetworkAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2019, 1, 1)   # Set Start Date
        self.set_end_date(2020, 4, 1)     # Set End Date
        self.set_cash(100000)            # Set Strategy Cash

        self.model_by_symbol = {}

        for ticker in ["SPY", "QQQ", "TLT"]:
            symbol = self.add_equity(ticker).symbol

            # Read the model saved in the ObjectStore
            for kvp in self.object_store:
                key = f'{symbol}_model'
                if not key == kvp.key or kvp.value is None:
                    continue
                file_path = self.object_store.get_file_path(kvp.key)
                self.model_by_symbol[symbol] = keras.models.load_model(file_path)
                self.debug(f'Model for {symbol} sucessfully retrieved. File {file_path}. Size {kvp.value.length}. Weights {self.model_by_symbol[symbol].get_weights()}')

        # Look-back period for training set
        self.lookback = 30

        # Train Neural Network every monday
        self.train(
            self.date_rules.every(DayOfWeek.MONDAY),
            self.time_rules.after_market_open("SPY"),
            self.neural_network_training)

        # Place trades on Monday, 30 minutes after the market is open
        self.schedule.on(
            self.date_rules.every_day("SPY"),
            self.time_rules.after_market_open("SPY", 30),
            self.trade)


    def on_end_of_algorithm(self):
        ''' Save the data and the mode using the ObjectStore '''
        for symbol, model in self.model_by_symbol.items():
            key = f'{symbol}_model'
            file = self.object_store.get_file_path(key)
            model.save(file)
            self.object_store.save(key)
            self.debug(f'Model for {symbol} sucessfully saved in the ObjectStore')


    def neural_network_training(self):
        '''Train the Neural Network and save the model in the ObjectStore'''
        symbols = self.securities.keys()

        # Daily historical data is used to train the machine learning model
        history = self.history(symbols, self.lookback + 1, Resolution.DAILY)
        history = history.open.unstack(0)

        for symbol in symbols:
            if symbol not in history:
                continue

            predictor = history[symbol][:-1]
            predictand = history[symbol][1:]

            # build a neural network from the 1st layer to the last layer
            model = Sequential()

            model.add(Dense(10, input_dim = 1))
            model.add(Activation('relu'))
            model.add(Dense(1))

            sgd = SGD(learning_rate = 0.01)   # learning rate = 0.01

            # choose loss function and optimizing method
            model.compile(loss='mse', optimizer=sgd)

            # pick an iteration number large enough for convergence
            for step in range(200):
                # training the model
                cost = model.train_on_batch(predictor, predictand)

            self.model_by_symbol[symbol] = model

    def trade(self):
        '''
        Predict the price using the trained model and out-of-sample data
        Enter or exit positions based on relationship of the open price of the current bar and the prices defined by the machine learning model.
        Liquidate if the open price is below the sell price and buy if the open price is above the buy price
        '''
        target = 1 / len(self.securities)

        for symbol, model in self.model_by_symbol.items():

            # Get the out-of-sample history
            history = self.history(symbol, self.lookback, Resolution.DAILY)
            history = history.open.unstack(0)[symbol]

            # Get the final predicted price
            prediction = model.predict(history)[0][-1]
            history_std = np.std(history)

            holding = self.portfolio[symbol]
            open_price = self.current_slice[symbol].open

            # Follow the trend
            if holding.invested:
                if open_price < prediction - history_std:
                    self.liquidate(symbol)
            else:
                if open_price > prediction + history_std:
                    self.set_holdings(symbol, target)
