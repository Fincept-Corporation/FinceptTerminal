# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-59CC070D
# Category: Futures
# Description: This algorithm tests and demonstrates EUREX futures subscription and trading: - It tests contracts rollover by adding...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This algorithm tests and demonstrates EUREX futures subscription and trading:
### - It tests contracts rollover by adding a continuous future and asserting that mapping happens at some point.
### - It tests basic trading by buying a contract and holding it until expiration.
### - It tests delisting and asserts the holdings are liquidated after that.
### </summary>
class BasicTemplateEurexFuturesAlgorithm(QCAlgorithm):
    def __init__(self):
        super().__init__()
        self._continuous_contract = None
        self._mapped_symbol = None
        self._contract_to_trade = None
        self._mappings_count = 0
        self._bought_quantity = 0
        self._liquidated_quantity = 0
        self._delisted = False

    def initialize(self):
        self.set_start_date(2024, 5, 30)
        self.set_end_date(2024, 6, 23)

        self.set_account_currency(Currencies.EUR);
        self.set_cash(1000000)

        self._continuous_contract = self.add_future(
            Futures.Indices.EURO_STOXX_50,
            Resolution.MINUTE,
            data_normalization_mode=DataNormalizationMode.BACKWARDS_RATIO,
            data_mapping_mode=DataMappingMode.FIRST_DAY_MONTH,
            contract_depth_offset=0,
        )
        self._continuous_contract.set_filter(timedelta(days=0), timedelta(days=180))
        self._mapped_symbol = self._continuous_contract.mapped

        benchmark = self.add_index("SX5E", market=Market.EUREX)
        self.set_benchmark(benchmark.symbol)

        func_seeder = FuncSecuritySeeder(self.get_last_known_prices)
        self.set_security_initializer(lambda security: func_seeder.seed_security(security))

    def on_data(self, slice):
        for changed_event in slice.symbol_changed_events.values():
            self._mappings_count += 1
            if self._mappings_count > 1:
                raise Exception(f"{self.time} - Unexpected number of symbol changed events (mappings): {self._mappings_count}. Expected only 1.")

            self.debug(f"{self.time} - SymbolChanged event: {changed_event}")

            if changed_event.old_symbol != str(self._mapped_symbol.id):
                raise Exception(f"{self.time} - Unexpected symbol changed event old symbol: {changed_event}")

            if changed_event.new_symbol != str(self._continuous_contract.mapped.id):
                raise Exception(f"{self.time} - Unexpected symbol changed event new symbol: {changed_event}")

            # Let's trade the previous mapped contract, so we can hold it until expiration for testing
            # (will be sooner than the new mapped contract)
            self._contract_to_trade = self._mapped_symbol
            self._mapped_symbol = self._continuous_contract.mapped

        # Let's trade after the mapping is done
        if self._contract_to_trade is not None and self._bought_quantity == 0 and self.securities[self._contract_to_trade].exchange.exchange_open:
            self.buy(self._contract_to_trade, 1)

        if self._contract_to_trade is not None and slice.delistings.contains_key(self._contract_to_trade):
            delisting = slice.delistings[self._contract_to_trade]
            if delisting.type == DelistingType.DELISTED:
                self._delisted = True

                if self.portfolio.invested:
                    raise Exception(f"{self.time} - Portfolio should not be invested after the traded contract is delisted.")

    def on_order_event(self, order_event):
        if order_event.symbol != self._contract_to_trade:
            raise Exception(f"{self.time} - Unexpected order event symbol: {order_event.symbol}. Expected {self._contract_to_trade}")

        if order_event.direction == OrderDirection.BUY:
            if order_event.status == OrderStatus.FILLED:
                if self._bought_quantity != 0 and self._liquidated_quantity != 0:
                    raise Exception(f"{self.time} - Unexpected buy order event status: {order_event.status}")

                self._bought_quantity = order_event.quantity
        elif order_event.direction == OrderDirection.SELL:
            if order_event.status == OrderStatus.FILLED:
                if self._bought_quantity <= 0 and self._liquidated_quantity != 0:
                    raise Exception(f"{self.time} - Unexpected sell order event status: {order_event.status}")

                self._liquidated_quantity = order_event.quantity
                if self._liquidated_quantity != -self._bought_quantity:
                    raise Exception(f"{self.time} - Unexpected liquidated quantity: {self._liquidated_quantity}. Expected: {-self._bought_quantity}")

    def on_securities_changed(self, changes):
        for added_security in changes.added_securities:
            if added_security.symbol.security_type == SecurityType.FUTURE and added_security.symbol.is_canonical():
                self._mapped_symbol = self._continuous_contract.mapped

    def on_end_of_algorithm(self):
        if self._mappings_count == 0:
            raise Exception(f"Unexpected number of symbol changed events (mappings): {self._mappings_count}. Expected 1.")

        if not self._delisted:
            raise Exception("Contract was not delisted")

        # Make sure we traded and that the position was liquidated on delisting
        if self._bought_quantity <= 0 or self._liquidated_quantity >= 0:
            raise Exception(f"Unexpected sold quantity: {self._bought_quantity} and liquidated quantity: {self._liquidated_quantity}")
