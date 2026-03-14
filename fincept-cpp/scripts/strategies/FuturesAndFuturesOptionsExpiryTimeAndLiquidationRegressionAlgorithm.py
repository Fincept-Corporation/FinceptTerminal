# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-2549D577
# Category: Options
# Description: Tests delistings for Futures and Futures Options to ensure that they are delisted at the expected times
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Tests delistings for Futures and Futures Options to ensure that they are delisted at the expected times.
### </summary>
class FuturesAndFutures_optionsExpiryTimeAndLiquidationRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.invested = False
        self.liquidated = 0
        self.delistings_received = 0

        self.expected_expiry_warning_time = datetime(2020, 6, 19)
        self.expected_expiry_delisting_time = datetime(2020, 6, 20)
        self.expected_liquidation_time = datetime(2020, 6, 20)

        self.set_start_date(2020, 1, 5)
        self.set_end_date(2020, 12, 1)
        self.set_cash(100000)

        es = Symbol.create_future(
            "ES",
            Market.CME,
            datetime(2020, 6, 19)
        )

        es_option = Symbol.create_option(
            es,
            Market.CME,
            OptionStyle.AMERICAN,
            OptionRight.PUT,
            3400.0,
            datetime(2020, 6, 19)
        )

        self.es_future = self.add_future_contract(es, Resolution.MINUTE).symbol
        self.es_future_option = self.add_future_option_contract(es_option, Resolution.MINUTE).symbol

    def on_data(self, data: Slice):
        for delisting in data.delistings.values():
            self.delistings_received += 1

            if delisting.type == DelistingType.WARNING and delisting.time != self.expected_expiry_warning_time:
                raise AssertionError(f"Expiry warning with time {delisting.time} but is expected to be {self.expected_expiry_warning_time}")

            if delisting.type == DelistingType.WARNING and delisting.time != datetime(self.time.year, self.time.month, self.time.day):
                raise AssertionError(f"Delisting warning received at an unexpected date: {self.time} - expected {delisting.time}")

            if delisting.type == DelistingType.DELISTED and delisting.time != self.expected_expiry_delisting_time:
                raise AssertionError(f"Delisting occurred at unexpected time: {delisting.time} - expected: {self.expected_expiry_delisting_time}")

            if delisting.type == DelistingType.DELISTED and delisting.time != datetime(self.time.year, self.time.month, self.time.day):
                raise AssertionError(f"Delisting notice received at an unexpected date: {self.time} - expected {delisting.time}")

        if not self.invested and \
            (self.es_future in data.bars or self.es_future in data.quote_bars) and \
            (self.es_future_option in data.bars or self.es_future_option in data.quote_bars):

            self.invested = True

            self.market_order(self.es_future, 1)
            self.market_order(self.es_future_option, 1)

    def on_order_event(self, order_event: OrderEvent):
        if order_event.direction != OrderDirection.SELL or order_event.status != OrderStatus.FILLED:
            return

        # * Future Liquidation
        # * Future Option Exercise
        # * We expect NO Underlying Future Liquidation because we already hold a Long future position so the FOP Put selling leaves us breakeven
        self.liquidated += 1
        if order_event.symbol.security_type == SecurityType.FUTURE_OPTION and self.expected_liquidation_time != self.time:
            raise AssertionError(f"Expected to liquidate option {order_event.symbol} at {self.expected_liquidation_time}, instead liquidated at {self.time}")

        if order_event.symbol.security_type == SecurityType.FUTURE and \
            (self.expected_liquidation_time - timedelta(minutes=1)) != self.time and \
            self.expected_liquidation_time != self.time:

            raise AssertionError(f"Expected to liquidate future {order_event.symbol} at {self.expected_liquidation_time} (+1 minute), instead liquidated at {self.time}")

    def on_end_of_algorithm(self):
        if not self.invested:
            raise AssertionError("Never invested in ES futures and FOPs")

        if self.delistings_received != 4:
            raise AssertionError(f"Expected 4 delisting events received, found: {self.delistings_received}")

        if self.liquidated != 2:
            raise AssertionError(f"Expected 3 liquidation events, found {self.liquidated}")
