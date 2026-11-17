# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

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
