# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D2180B48
# Category: General Strategy
# Description: N L T K Sentiment Trading Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

import nltk
# for details of NLTK, please visit https://www.nltk.org/index.html

class NLTKSentimentTradingAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2018, 1, 1)  # Set Start Date
        self.set_end_date(2019, 1, 1) # Set End Date
        self.set_cash(100000)  # Set Strategy Cash
        
        spy = self.add_equity("SPY", Resolution.MINUTE)
        self.text = self.get_text() # Get custom text data for creating trading signals
        self._symbols = [spy.symbol] # This can be extended to multiple symbols
        
        # for what extra models needed to download, please use code nltk.download()
        nltk.download('punkt')
        self.schedule.on(self.date_rules.every_day("SPY"), self.time_rules.after_market_open("SPY", 30), self.trade)
    
    def trade(self):
        current_time = f'{self.time.year}-{self.time.month}-{self.time.day}'
        current_text = self.text.loc[current_time][0]
        words = nltk.word_tokenize(current_text)
        
        # users should decide their own positive and negative words
        positive_word = 'Up'
        negative_word = 'Down'
        
        for holding in self.portfolio.values:
            # liquidate if it contains negative words
            if negative_word in words and holding.invested:
                self.liquidate(holding.symbol)
            
            # buy if it contains positive words
            if positive_word in words and not holding.invested:
                self.set_holdings(holding.symbol, 1 / len(self._symbols))
        
    def get_text(self):
        # import custom data
        # Note: dl must be 1, or it will not download automatically
        url = 'https://www.dropbox.com/s/7xgvkypg6uxp6xl/EconomicNews.csv?dl=1'
        data = self.download(url).split('\n')

        headline = [x.split(',')[1] for x in data][1:]
        date = [x.split(',')[0] for x in data][1:]
        
        # create a pd dataframe with 1st col being date and 2nd col being headline (content of the text)
        df = pd.DataFrame(headline, index = date, columns = ['headline'])
        return df
