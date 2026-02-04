# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-34321EAB
# Category: Futures
# Description: Basic Continuous Futures Template Algorithm with extended market hours
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Basic Continuous Futures Template Algorithm with extended market hours
### </summary>
class BasicTemplateContinuousFutureWithExtendedMarketAlgorithm(QCAlgorithm):
    '''Basic template algorithm simply initializes the date range and cash'''

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013, 7, 1)
        self.set_end_date(2014, 1, 1)

        self._continuous_contract = self.add_future(Futures.Indices.SP_500_E_MINI,
                                                  data_normalization_mode = DataNormalizationMode.BACKWARDS_RATIO,
                                                  data_mapping_mode = DataMappingMode.LAST_TRADING_DAY,
                                                  contract_depth_offset = 0,
                                                  extended_market_hours = True)

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

        if not self.is_market_open(self._continuous_contract.symbol):
            return

        if not self.portfolio.invested:
            if self._fast.current.value > self._slow.current.value:
                self._current_contract = self.securities[self._continuous_contract.mapped]
                self.buy(self._current_contract.symbol, 1)
        elif self._fast.current.value < self._slow.current.value:
            self.liquidate()

        if self._current_contract is not None and self._current_contract.symbol != self._continuous_contract.mapped:
            self.log(f"{Time} - rolling position from {self._current_contract.symbol} to {self._continuous_contract.mapped}")

            current_position_size = self._current_contract.holdings.quantity
            self.liquidate(self._current_contract.symbol)
            self.buy(self._continuous_contract.mapped, current_position_size)
            self._current_contract = self.securities[self._continuous_contract.mapped]

    def on_order_event(self, order_event):
        self.debug("Purchased Stock: {0}".format(order_event.symbol))

    def on_securities_changed(self, changes):
        self.debug(f"{self.time}-{changes}")
