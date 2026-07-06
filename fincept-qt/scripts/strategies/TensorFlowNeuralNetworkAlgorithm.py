# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-42FC8E69
# Category: Machine Learning
# Description: Enter or exit positions based on relationship of the open price of the current bar and the prices defined by the mach...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
import tensorflow.compat.v1 as tf

class TensorFlowNeuralNetworkAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)  # Set Start Date
        self.set_end_date(2013, 10, 8) # Set End Date
        
        self.set_cash(100000)  # Set Strategy Cash
        spy = self.add_equity("SPY", Resolution.MINUTE) # Add Equity
        
        self.symbols = [spy.symbol] # potential trading symbols pool (in this algorithm there is only 1). 
        self.lookback = 30 # number of previous days for training
        
        self.schedule.on(self.date_rules.every(DayOfWeek.MONDAY), self.time_rules.after_market_open("SPY", 28), self.net_train) # train the neural network 28 mins after market open
        self.schedule.on(self.date_rules.every(DayOfWeek.MONDAY), self.time_rules.after_market_open("SPY", 30), self.trade) # trade 30 mins after market open
        
    def add_layer(self, inputs, in_size, out_size, activation_function=None):
        # add one more layer and return the output of this layer
        # this is one NN with only one hidden layer
        weights = tf.Variable(tf.random_normal([in_size, out_size]))
        biases = tf.Variable(tf.zeros([1, out_size]) + 0.1)
        wx_plus_b = tf.matmul(inputs, weights) + biases
        if activation_function is None:
            outputs = wx_plus_b
        else:
            outputs = activation_function(wx_plus_b)
        return outputs
    
    def net_train(self):
        # Daily historical data is used to train the machine learning model
        history = self.history(self.symbols, self.lookback + 1, Resolution.DAILY)
        
        # model: use prices_x to fit prices_y; key: symbol; value: according price
        self.prices_x, self.prices_y = {}, {}
        
        # key: symbol; values: prices for sell or buy 
        self.sell_prices, self.buy_prices = {}, {}
        
        for symbol in self.symbols:
            if not history.empty:
                # Daily historical data is used to train the machine learning model 
                # use open prices to predict the next days'
                self.prices_x[symbol] = list(history.loc[symbol.value]['open'][:-1])
                self.prices_y[symbol] = list(history.loc[symbol.value]['open'][1:])
        
        for symbol in self.symbols:
            if symbol in self.prices_x:
                # create numpy array
                x_data = np.array(self.prices_x[symbol]).astype(np.float32).reshape((-1,1))
                y_data = np.array(self.prices_y[symbol]).astype(np.float32).reshape((-1,1))
                
                # define placeholder for inputs to network
                tf.disable_v2_behavior()
                xs = tf.placeholder(tf.float32, [None, 1])
                ys = tf.placeholder(tf.float32, [None, 1])
                
                # add hidden layer
                l1 = self.add_layer(xs, 1, 10, activation_function=tf.nn.relu)
                # add output layer
                prediction = self.add_layer(l1, 10, 1, activation_function=None)
                
                # the error between prediciton and real data
                loss = tf.reduce_mean(tf.reduce_sum(tf.square(ys - prediction),
                                     reduction_indices=[1]))
                # use gradient descent and square error
                train_step = tf.train.GradientDescentOptimizer(0.1).minimize(loss)
                
                # the following is precedure for tensorflow
                sess = tf.Session()
                
                init = tf.global_variables_initializer()
                sess.run(init)
                
                for i in range(200):
                    # training
                    sess.run(train_step, feed_dict={xs: x_data, ys: y_data})
            
            # predict today's price
            y_pred_final = sess.run(prediction, feed_dict = {xs: y_data})[0][-1]
            
            # get sell prices and buy prices as trading signals
            self.sell_prices[symbol] = y_pred_final - np.std(y_data)
            self.buy_prices[symbol] = y_pred_final + np.std(y_data)
        
    def trade(self):
        ''' 
        Enter or exit positions based on relationship of the open price of the current bar and the prices defined by the machine learning model.
        Liquidate if the open price is below the sell price and buy if the open price is above the buy price 
        ''' 
        for holding in self.portfolio.Values:
            if self.current_slice[holding.symbol].open < self.sell_prices[holding.symbol] and holding.invested:
                self.liquidate(holding.symbol)
            
            if self.current_slice[holding.symbol].open > self.buy_prices[holding.symbol] and not holding.invested:
                self.set_holdings(holding.symbol, 1 / len(self.symbols))
