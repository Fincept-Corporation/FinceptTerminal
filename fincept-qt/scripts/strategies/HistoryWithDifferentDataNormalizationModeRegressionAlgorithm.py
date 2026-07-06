# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-25201E5E
# Category: Regression Test
# Description: Regression algorithm illustrating how to request history data for different data normalization modes
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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
