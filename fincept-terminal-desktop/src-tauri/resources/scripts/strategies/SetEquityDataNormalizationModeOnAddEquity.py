# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-D64ED04F
# Category: General Strategy
# Description: This regression algorithm has examples of how to add an equity indicating the <see cref="DataNormalizationMode"/> dir...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm has examples of how to add an equity indicating the <see cref="DataNormalizationMode"/>
### directly with the <see cref="QCAlgorithm.add_equity"/> method instead of using the <see cref="Equity.SET_DATA_NORMALIZATION_MODE"/> method.
### </summary>
class SetEquityDataNormalizationModeOnAddEquity(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 7)

        spy_normalization_mode = DataNormalizationMode.RAW
        ibm_normalization_mode = DataNormalizationMode.ADJUSTED
        aig_normalization_mode = DataNormalizationMode.TOTAL_RETURN

        self._price_ranges = {}

        spy_equity = self.add_equity("SPY", Resolution.MINUTE, data_normalization_mode=spy_normalization_mode)
        self.check_equity_data_normalization_mode(spy_equity, spy_normalization_mode)
        self._price_ranges[spy_equity] = (167.28, 168.37)

        ibm_equity = self.add_equity("IBM", Resolution.MINUTE, data_normalization_mode=ibm_normalization_mode)
        self.check_equity_data_normalization_mode(ibm_equity, ibm_normalization_mode)
        self._price_ranges[ibm_equity] = (135.864131052, 136.819606508)

        aig_equity = self.add_equity("AIG", Resolution.MINUTE, data_normalization_mode=aig_normalization_mode)
        self.check_equity_data_normalization_mode(aig_equity, aig_normalization_mode)
        self._price_ranges[aig_equity] = (48.73, 49.10)

    def on_data(self, slice):
        for equity, (min_expected_price, max_expected_price) in self._price_ranges.items():
            if equity.has_data and (equity.price < min_expected_price or equity.price > max_expected_price):
                raise Exception(f"{equity.symbol}: Price {equity.price} is out of expected range [{min_expected_price}, {max_expected_price}]")

    def check_equity_data_normalization_mode(self, equity, expected_normalization_mode):
        subscriptions = [x for x in self.subscription_manager.subscriptions if x.symbol == equity.symbol]
        if any([x.data_normalization_mode != expected_normalization_mode for x in subscriptions]):
            raise Exception(f"Expected {equity.symbol} to have data normalization mode {expected_normalization_mode} but was {subscriptions[0].data_normalization_mode}")


