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


