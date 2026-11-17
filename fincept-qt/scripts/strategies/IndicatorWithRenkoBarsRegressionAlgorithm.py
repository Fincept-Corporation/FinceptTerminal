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
### Regrssion algorithm to assert we can update indicators that inherit from IndicatorBase<TradeBar> with RenkoBar's
### </summary>
### <meta name="tag" content="renko" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="using data" />
### <meta name="tag" content="consolidating data" />
class IndicatorWithRenkoBarsRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 9)

        self.add_equity("SPY")
        self.add_equity("AIG")

        spy_renko_consolidator = RenkoConsolidator(0.1)
        spy_renko_consolidator.data_consolidated += self.on_s_p_y_data_consolidated

        aig_renko_consolidator = RenkoConsolidator(0.05)
        aig_renko_consolidator.data_consolidated += self.on_a_i_g_data_consolidated

        self.subscription_manager.add_consolidator("SPY", spy_renko_consolidator)
        self.subscription_manager.add_consolidator("AIG", aig_renko_consolidator)

        self._mi = MassIndex("MassIndex", 9, 25)
        self._wasi = WilderAccumulativeSwingIndex("WilderAccumulativeSwingIndex", 8)
        self._wsi = WilderSwingIndex("WilderSwingIndex", 8)
        self._b = Beta("Beta", 3, "AIG", "SPY")
        self._indicators = [self._mi, self._wasi, self._wsi, self._b]

    def on_s_p_y_data_consolidated(self, sender, renko_bar):
        self._mi.update(renko_bar)
        self._wasi.update(renko_bar)
        self._wsi.update(renko_bar)
        self._b.update(renko_bar)

    def on_a_i_g_data_consolidated(self, sender, renko_bar):
        self._b.update(renko_bar)

    def on_end_of_algorithm(self):
        for indicator in self._indicators:
            if not indicator.is_ready:
                raise Exception(f"{indicator.name} indicator should be ready")
            elif indicator.current.value == 0:
                raise Exception(f"The current value of the {indicator.name} indicator should be different than zero")
