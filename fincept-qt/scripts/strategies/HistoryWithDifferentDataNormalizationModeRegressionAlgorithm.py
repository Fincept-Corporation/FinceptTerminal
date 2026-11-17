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

### <summary>
### Regression algorithm illustrating how to request history data for different data normalization modes.
### </summary>
class HistoryWithDifferentDataNormalizationModeRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2014, 1, 1)
        self.aapl_equity_symbol = self.add_equity("AAPL", Resolution.DAILY).symbol
        self.es_future_symbol = self.add_future(Futures.Indices.SP_500_E_MINI, Resolution.DAILY).symbol

    def on_end_of_algorithm(self):
        equity_data_normalization_modes = [
            DataNormalizationMode.RAW,
            DataNormalizationMode.ADJUSTED,
            DataNormalizationMode.SPLIT_ADJUSTED
        ]
        self.check_history_results_for_data_normalization_modes(self.aapl_equity_symbol, self.start_date, self.end_date, Resolution.DAILY,
            equity_data_normalization_modes)

        future_data_normalization_modes = [
            DataNormalizationMode.RAW,
            DataNormalizationMode.BACKWARDS_RATIO,
            DataNormalizationMode.BACKWARDS_PANAMA_CANAL,
            DataNormalizationMode.FORWARD_PANAMA_CANAL
        ]
        self.check_history_results_for_data_normalization_modes(self.es_future_symbol, self.start_date, self.end_date, Resolution.DAILY,
            future_data_normalization_modes)

    def check_history_results_for_data_normalization_modes(self, symbol, start, end, resolution, data_normalization_modes):
        history_results = [self.history([symbol], start, end, resolution, data_normalization_mode=x) for x in data_normalization_modes]
        history_results = [x.droplevel(0, axis=0) for x in history_results] if len(history_results[0].index.levels) == 3 else history_results
        history_results = [x.loc[symbol].close for x in history_results]

        if any(x.size == 0 or x.size != history_results[0].size for x in history_results):
            raise Exception(f"History results for {symbol} have different number of bars")

        # Check that, for each history result, close prices at each time are different for these securities (AAPL and ES)
        for j in range(history_results[0].size):
            close_prices = set(history_results[i][j] for i in range(len(history_results)))
            if len(close_prices) != len(data_normalization_modes):
                raise Exception(f"History results for {symbol} have different close prices at the same time")
