# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D0F1B141
# Category: Indicators
# Description: Talib Indicators Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
import talib

class CalibratedResistanceAtmosphericScrubbers(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 1, 2)
        self.set_end_date(2020, 1, 6) 
        self.set_cash(100000) 
        self.add_equity("SPY", Resolution.HOUR)
        
        self.rolling_window = pd.DataFrame()
        self.dema_period = 3
        self.sma_period = 3
        self.wma_period = 3
        self.window_size = self.dema_period * 2
        self.set_warm_up(self.window_size)
        
    def on_data(self, data):
        if "SPY" not in data.bars:
            return
        
        close = data["SPY"].close
        
        if self.is_warming_up:
            # Add latest close to rolling window
            row = pd.DataFrame({"close": [close]}, index=[data.time])
            self.rolling_window = pd.concat([self.rolling_window, row]).iloc[-self.window_size:]
            
            # If we have enough closing data to start calculating indicators...
            if self.rolling_window.shape[0] == self.window_size:
                closes = self.rolling_window['close'].values
                
                # Add indicator columns to DataFrame
                self.rolling_window['DEMA'] = talib.DEMA(closes, self.dema_period)
                self.rolling_window['EMA'] = talib.EMA(closes, self.sma_period)
                self.rolling_window['WMA'] = talib.WMA(closes, self.wma_period)
            return
        
        closes = np.append(self.rolling_window['close'].values, close)[-self.window_size:]
        
        # Update talib indicators time series with the latest close
        row = pd.DataFrame({"close": close,
                            "DEMA" : talib.DEMA(closes, self.dema_period)[-1],
                            "EMA"  : talib.EMA(closes, self.sma_period)[-1],
                            "WMA"  : talib.WMA(closes, self.wma_period)[-1]},
                            index=[data.time])
        
        self.rolling_window = pd.concat([self.rolling_window, row]).iloc[-self.window_size:]

        
    def on_end_of_algorithm(self):
        self.log(f"\nRolling Window:\n{self.rolling_window.to_string()}\n")
        self.log(f"\nLatest Values:\n{self.rolling_window.iloc[-1].to_string()}\n")
