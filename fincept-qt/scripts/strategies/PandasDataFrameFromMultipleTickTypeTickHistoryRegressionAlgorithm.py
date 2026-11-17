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
### Regression algorithm for asserting that tick history that includes multiple tick types (trade, quote) is correctly converted to a pandas
### dataframe without raising exceptions. The main exception in this case was a "non-unique multi-index" error due to trades adn quote ticks with
### duplicated timestamps.
### </summary>
class PandasDataFrameFromMultipleTickTypeTickHistoryRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 10, 8)
        self.set_end_date(2013, 10, 8)

        spy = self.add_equity("SPY", Resolution.MINUTE).symbol

        subscriptions = [x for x in self.subscription_manager.subscriptions if x.symbol == spy]
        if len(subscriptions) != 2:
            raise Exception(f"Expected 2 subscriptions, but found {len(subscriptions)}")

        history = pd.DataFrame()
        try:
            history = self.history(Tick, spy, timedelta(days=1), Resolution.TICK)
        except Exception as e:
            raise Exception(f"History call failed: {e}")

        if history.shape[0] == 0:
            raise Exception("SPY tick history is empty")

        if not np.array_equal(history.columns.to_numpy(), ['askprice', 'asksize', 'bidprice', 'bidsize', 'exchange', 'lastprice', 'quantity']):
            raise Exception("Unexpected columns in SPY tick history")

        self.quit()
