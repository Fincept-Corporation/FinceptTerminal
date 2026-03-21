# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C573FB46
# Category: Futures
# Description: Continuous Futures Regression algorithm. Asserting and showcasing the behavior of adding a continuous future
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Continuous Futures Regression algorithm. Asserting and showcasing the behavior of adding a continuous future
### </summary>
class ContinuousFutureRegressionAlgorithm(QCAlgorithm):
    '''Basic template algorithm simply initializes the date range and cash'''

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013, 7, 1)
        self.set_end_date(2014, 1, 1)

        self._previous_mapped_contract_symbols = []
        self._last_date_log = -1
        self._continuous_contract = self.add_future(Futures.Indices.SP_500_E_MINI,
                                                  data_normalization_mode = DataNormalizationMode.BACKWARDS_RATIO,
                                                  data_mapping_mode = DataMappingMode.LAST_TRADING_DAY,
                                                  contract_depth_offset= 0)
        self._current_mapped_symbol = self._continuous_contract.symbol

    def on_data(self, data):
        '''on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        currently_mapped_security = self.securities[self._continuous_contract.mapped]
        if len(data.keys()) != 1:
            raise ValueError(f"We are getting data for more than one symbols! {','.join(data.keys())}")

        for changed_event in data.symbol_changed_events.values():
            if changed_event.symbol == self._continuous_contract.symbol:
                self._previous_mapped_contract_symbols.append(self.symbol(changed_event.old_symbol))
                self.log(f"SymbolChanged event: {changed_event}")

                if self._current_mapped_symbol == self._continuous_contract.mapped:
                    raise ValueError(f"Continuous contract current symbol did not change! {self._continuous_contract.mapped}")

        if self._last_date_log != self.time.month and currently_mapped_security.has_data:
            self._last_date_log = self.time.month

            self.log(f"{self.time}- {currently_mapped_security.get_last_data()}")
            if self.portfolio.invested:
                self.liquidate()
            else:
                # This works because we set this contract as tradable, even if it's a canonical security
                self.buy(currently_mapped_security.symbol, 1)

            if self.time.month == 1 and self.time.year == 2013:
                response = self.history( [ self._continuous_contract.symbol ], 60 * 24 * 90)
                if response.empty:
                    raise ValueError("Unexpected empty history response")

        self._current_mapped_symbol = self._continuous_contract.mapped

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug("Purchased Stock: {0}".format(order_event.symbol))

    def on_securities_changed(self, changes):
        self.debug(f"{self.time}-{changes}")

    def on_end_of_algorithm(self):
        expected_mapping_counts = 2
        if len(self._previous_mapped_contract_symbols) != expected_mapping_counts:
            raise ValueError(f"Unexpected symbol changed events: {len(self._previous_mapped_contract_symbols)}, was expecting {expected_mapping_counts}")

        delisted_securities = [
            [security for security in self.securities.total if security.symbol == symbol][0]
            for symbol in self._previous_mapped_contract_symbols
        ]
        marked_delisted_securities = [security for security in delisted_securities if security.is_delisted and not security.is_tradable]
        if len(marked_delisted_securities) != len(delisted_securities):
            raise ValueError(f"Not all delisted contracts are properly marked as delisted and non-tradable: "
                             f"only {len(marked_delisted_securities)} are marked, was expecting {len(delisted_securities)}")
