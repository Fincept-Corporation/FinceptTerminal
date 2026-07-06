# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D3112758
# Category: ETF
# Description: Strategy example using a portfolio of ETF Global Rotation
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Strategy example using a portfolio of ETF Global Rotation
### </summary>
### <meta name="tag" content="strategy example" />
### <meta name="tag" content="momentum" />
### <meta name="tag" content="using data" />

### <summary>
### Strategy example using a portfolio of ETF Global Rotation
### </summary>
### <meta name="tag" content="strategy example" />
### <meta name="tag" content="momentum" />
### <meta name="tag" content="using data" />
class ETFGlobalRotationAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_cash(25000)
        self.set_start_date(2007,1,1)
        self.last_rotation_time = datetime.min
        self.rotation_interval = timedelta(days=30)
        self.first = True

        # these are the growth symbols we'll rotate through
        growth_symbols =["MDY", # US S&P mid cap 400
                        "IEV", # i_shares S&P europe 350
                        "EEM", # i_shared MSCI emerging markets
                        "ILF", # i_shares S&P latin america
                        "EPP" ] # i_shared MSCI Pacific ex-Japan

        # these are the safety symbols we go to when things are looking bad for growth
        safety_symbols = ["EDV", "SHY"] # "EDV" Vangaurd TSY 25yr, "SHY" Barclays Low Duration TSY
        # we'll hold some computed data in these guys
        self.symbol_data = []
        for symbol in list(set(growth_symbols) | set(safety_symbols)):
            self.add_security(SecurityType.EQUITY, symbol, Resolution.MINUTE)
            self.one_month_performance = self.mom(symbol, 30, Resolution.DAILY)
            self.three_month_performance = self.mom(symbol, 90, Resolution.DAILY)
            self.symbol_data.append([symbol, self.one_month_performance, self.three_month_performance])
    
        
    def on_data(self, data):
        # the first time we come through here we'll need to do some things such as allocation
        # and initializing our symbol data

        if self.first:
            self.first = False
            self.last_rotation_time = self.time
            return
        delta = self.time - self.last_rotation_time
        if delta > self.rotation_interval:
            self.last_rotation_time = self.time

            ordered_obj_scores = sorted(self.symbol_data, key=lambda x: Score(x[1].current.value,x[2].current.value).objective_score(), reverse=True)
            for x in ordered_obj_scores:
                self.log(">>SCORE>>" + x[0] + ">>" + str(Score(x[1].current.value,x[2].current.value).objective_score()))
            # pick which one is best from growth and safety symbols
            best_growth = ordered_obj_scores[0]
            if Score(best_growth[1].current.value,best_growth[2].current.value).objective_score() > 0:
                if (self.portfolio[best_growth[0]].quantity == 0):
                    self.log("PREBUY>>LIQUIDATE>>")
                    self.liquidate()
                self.log(">>BUY>>" + str(best_growth[0]) + "@" + str(100 * best_growth[1].current.value))
                qty = self.portfolio.margin_remaining / self.securities[best_growth[0]].close
                self.market_order(best_growth[0], int(qty)) 
            else:
            # if no one has a good objective score then let's hold cash this month to be safe
                self.log(">>LIQUIDATE>>CASH")
                self.liquidate()
        
class Score(object):
    
    def __init__(self,one_month_performance_value,three_month_performance_value):
        self.one_month_performance = one_month_performance_value
        self.three_month_performance = three_month_performance_value
    
    def objective_score(self):
        weight1 = 100
        weight2 = 75
        return (weight1 * self.one_month_performance + weight2 * self.three_month_performance) / (weight1 + weight2)
