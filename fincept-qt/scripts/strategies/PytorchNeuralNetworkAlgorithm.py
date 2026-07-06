# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3BE1BA87
# Category: Machine Learning
# Description: Enter or exit positions based on relationship of the open price of the current bar and the prices defined by the mach...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
import torch
import torch.nn.functional as F

class PytorchNeuralNetworkAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)  # Set Start Date
        self.set_end_date(2013, 10, 8) # Set End Date
        
        self.set_cash(100000)  # Set Strategy Cash
        
        # add symbol
        spy = self.add_equity("SPY", Resolution.MINUTE)
        self._symbols = [spy.symbol] # using a list can extend to condition for multiple symbols
        
        self.lookback = 30 # days of historical data (look back)
        
        self.schedule.on(self.date_rules.every_day("SPY"), self.time_rules.after_market_open("SPY", 28), self.net_train) # train the NN
        self.schedule.on(self.date_rules.every_day("SPY"), self.time_rules.after_market_open("SPY", 30), self.trade)
    
    def net_train(self):
        # Daily historical data is used to train the machine learning model
        history = self.history(self._symbols, self.lookback + 1, Resolution.DAILY)
        
        # dicts that store prices for training
        self.prices_x = {} 
        self.prices_y = {}
        
        # dicts that store prices for sell and buy
        self.sell_prices = {}
        self.buy_prices = {}
        
        for symbol in self._symbols:
            if not history.empty:
                # x: preditors; y: response
                self.prices_x[symbol] = list(history.loc[symbol.value]['open'])[:-1]
                self.prices_y[symbol] = list(history.loc[symbol.value]['open'])[1:]
                
        for symbol in self._symbols:
            # if this symbol has historical data
            if symbol in self.prices_x:
                
                net = Net(n_feature=1, n_hidden=10, n_output=1)     # define the network
                optimizer = torch.optim.SGD(net.parameters(), lr=0.2)
                loss_func = torch.nn.MSELoss()  # this is for regression mean squared loss
                
                for t in range(200):
                    # Get data and do preprocessing
                    x = torch.from_numpy(np.array(self.prices_x[symbol])).float()
                    y = torch.from_numpy(np.array(self.prices_y[symbol])).float()
                    
                    # unsqueeze data (see pytorch doc for details)
                    x = x.unsqueeze(1) 
                    y = y.unsqueeze(1)
                
                    prediction = net(x)     # input x and predict based on x

                    loss = loss_func(prediction, y)     # must be (1. nn output, 2. target)

                    optimizer.zero_grad()   # clear gradients for next train
                    loss.backward()         # backpropagation, compute gradients
                    optimizer.step()        # apply gradients
            
            # Follow the trend    
            self.buy_prices[symbol] = net(y)[-1] + np.std(y.data.numpy())
            self.sell_prices[symbol] = net(y)[-1] - np.std(y.data.numpy())
        
    def trade(self):
        ''' 
        Enter or exit positions based on relationship of the open price of the current bar and the prices defined by the machine learning model.
        Liquidate if the open price is below the sell price and buy if the open price is above the buy price 
        ''' 
        for holding in self.portfolio.values():
            if self.current_slice[holding.symbol].open < self.sell_prices[holding.symbol] and holding.invested:
                self.liquidate(holding.symbol)
            
            if self.current_slice[holding.symbol].open > self.buy_prices[holding.symbol] and not holding.invested:
                self.set_holdings(holding.symbol, 1 / len(self._symbols))

            
        
# class for Pytorch NN model
class Net(torch.nn.Module):
    def __init__(self, n_feature, n_hidden, n_output):
        super(Net, self).__init__()
        self.hidden = torch.nn.Linear(n_feature, n_hidden)   # hidden layer
        self.predict = torch.nn.Linear(n_hidden, n_output)   # output layer
    
    def forward(self, x):
        x = F.relu(self.hidden(x))      # activation function for hidden layer
        x = self.predict(x)             # linear output
        return x
