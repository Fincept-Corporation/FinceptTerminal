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
### Basic Continuous Futures Template Algorithm
### </summary>
class BasicTemplateContinuousFutureAlgorithm(QCAlgorithm):
    '''Basic template algorithm simply initializes the date range and cash'''

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013, 7, 1)
        self.set_end_date(2014, 1, 1)

        self._continuous_contract = self.add_future(Futures.Indices.SP_500_E_MINI,
                                                  data_normalization_mode = DataNormalizationMode.BACKWARDS_RATIO,
                                                  data_mapping_mode = DataMappingMode.LAST_TRADING_DAY,
                                                  contract_depth_offset = 0)

        self._fast = self.sma(self._continuous_contract.symbol, 4, Resolution.DAILY)
        self._slow = self.sma(self._continuous_contract.symbol, 10, Resolution.DAILY)
        self._current_contract = None

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        for changed_event in data.symbol_changed_events.values():
            if changed_event.symbol == self._continuous_contract.symbol:
                self.log(f"SymbolChanged event: {changed_event}")

        if not self.portfolio.invested:
            if self._fast.current.value > self._slow.current.value:
                self._current_contract = self.securities[self._continuous_contract.mapped]
                self.buy(self._current_contract.symbol, 1)
        elif self._fast.current.value < self._slow.current.value:
            self.liquidate()

        # We check exchange hours because the contract mapping can call OnData outside of regular hours.
        if self._current_contract is not None and self._current_contract.symbol != self._continuous_contract.mapped and self._continuous_contract.exchange.exchange_open:
            self.log(f"{self.time} - rolling position from {self._current_contract.symbol} to {self._continuous_contract.mapped}")

            current_position_size = self._current_contract.holdings.quantity
            self.liquidate(self._current_contract.symbol)
            self.buy(self._continuous_contract.mapped, current_position_size)
            self._current_contract = self.securities[self._continuous_contract.mapped]

    def on_order_event(self, order_event):
        self.debug("Purchased Stock: {0}".format(order_event.symbol))

    def on_securities_changed(self, changes):
        self.debug(f"{self.time}-{changes}")
