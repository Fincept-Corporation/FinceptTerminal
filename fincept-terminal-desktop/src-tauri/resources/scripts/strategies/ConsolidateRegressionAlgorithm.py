# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-16F5EE2E
# Category: Data Consolidation
# Description: Regression algorithm reproducing data type bugs in the Consolidate API. Related to GH 4205
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from CustomDataRegressionAlgorithm import Bitcoin

### <summary>
### Regression algorithm reproducing data type bugs in the Consolidate API. Related to GH 4205.
### </summary>
class ConsolidateRegressionAlgorithm(QCAlgorithm):

    # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    def initialize(self):
        self.set_start_date(2020, 1, 5)
        self.set_end_date(2020, 1, 20)

        SP500 = Symbol.create(Futures.Indices.SP_500_E_MINI, SecurityType.FUTURE, Market.CME)
        symbol = self.future_chain_provider.get_future_contract_list(SP500, self.start_date)[0]
        self._future = self.add_future_contract(symbol)

        tradable_dates_count = len(list(Time.each_tradeable_day_in_time_zone(self._future.exchange.hours,
                                                                             self.start_date,
                                                                             self.end_date,
                                                                             self._future.exchange.time_zone,
                                                                             False)));
        self._expected_consolidation_counts = [];

        self.consolidate(symbol, Calendar.MONTHLY, lambda bar: self.update_monthly_consolidator(bar, -1)) # shouldn't consolidate

        self.consolidate(symbol, Calendar.WEEKLY, TickType.TRADE, lambda bar: self.update_weekly_consolidator(bar))

        self.consolidate(symbol, Resolution.DAILY, lambda bar: self.update_trade_bar(bar, 0))
        self._expected_consolidation_counts.append(tradable_dates_count)

        self.consolidate(symbol, Resolution.DAILY, TickType.QUOTE, lambda bar: self.update_quote_bar(bar, 1))
        self._expected_consolidation_counts.append(tradable_dates_count)

        self.consolidate(symbol, timedelta(1), lambda bar: self.update_trade_bar(bar, 2))
        self._expected_consolidation_counts.append(tradable_dates_count - 1)

        self.consolidate(symbol, timedelta(1), TickType.QUOTE, lambda bar: self.update_quote_bar(bar, 3))
        self._expected_consolidation_counts.append(tradable_dates_count - 1)

        # sending None tick type
        self.consolidate(symbol, timedelta(1), None, lambda bar: self.update_trade_bar(bar, 4))
        self._expected_consolidation_counts.append(tradable_dates_count - 1)

        self.consolidate(symbol, Resolution.DAILY, None, lambda bar: self.update_trade_bar(bar, 5))
        self._expected_consolidation_counts.append(tradable_dates_count)

        self._consolidation_counts = [0] * len(self._expected_consolidation_counts)
        self._smas = [SimpleMovingAverage(10) for x in self._consolidation_counts]
        self._last_sma_updates = [datetime.min for x in self._consolidation_counts]
        self._monthly_consolidator_sma = SimpleMovingAverage(10)
        self._monthly_consolidation_count = 0
        self._weekly_consolidator_sma = SimpleMovingAverage(10)
        self._weekly_consolidation_count = 0
        self._last_weekly_sma_update = datetime.min

        # custom data
        self._custom_data_consolidator = 0
        custom_symbol = self.add_data(Bitcoin, "BTC", Resolution.DAILY).symbol
        self.consolidate(custom_symbol, timedelta(1), lambda bar: self.increment_counter(1))

        self._custom_data_consolidator2 = 0
        self.consolidate(custom_symbol, Resolution.DAILY, lambda bar: self.increment_counter(2))

    def increment_counter(self, id):
        if id == 1:
            self._custom_data_consolidator += 1
        if id == 2:
            self._custom_data_consolidator2 += 1

    def update_trade_bar(self, bar, position):
        self._smas[position].update(bar.end_time, bar.volume)
        self._last_sma_updates[position] = bar.end_time
        self._consolidation_counts[position] += 1

    def update_quote_bar(self, bar, position):
        self._smas[position].update(bar.end_time, bar.ask.high)
        self._last_sma_updates[position] = bar.end_time
        self._consolidation_counts[position] += 1

    def update_monthly_consolidator(self, bar):
        self._monthly_consolidator_sma.update(bar.end_time, bar.volume)
        self._monthly_consolidation_count += 1

    def update_weekly_consolidator(self, bar):
        self._weekly_consolidator_sma.update(bar.end_time, bar.volume)
        self._last_weekly_sma_update = bar.end_time
        self._weekly_consolidation_count += 1

    def on_end_of_algorithm(self):
        for i, expected_consolidation_count in enumerate(self._expected_consolidation_counts):
            consolidation_count = self._consolidation_counts[i]
            if consolidation_count != expected_consolidation_count:
                raise ValueError(f"Unexpected consolidation count for index {i}: expected {expected_consolidation_count} but was {consolidation_count}")

        expected_weekly_consolidations = (self.end_date - self.start_date).days // 7
        if self._weekly_consolidation_count != expected_weekly_consolidations:
            raise ValueError(f"Expected {expected_weekly_consolidations} weekly consolidations but found {self._weekly_consolidation_count}")

        if self._custom_data_consolidator == 0:
            raise ValueError("Custom data consolidator did not consolidate any data")

        if self._custom_data_consolidator2 == 0:
            raise ValueError("Custom data consolidator 2 did not consolidate any data")

        for i, sma in enumerate(self._smas):
            if sma.samples != self._expected_consolidation_counts[i]:
                raise Exception(f"Expected {self._expected_consolidation_counts[i]} samples in each SMA but found {sma.samples} in SMA in index {i}")

            last_update = self._last_sma_updates[i]
            if sma.current.time != last_update:
                raise Exception(f"Expected SMA in index {i} to have been last updated at {last_update} but was {sma.current.time}")

        if self._monthly_consolidation_count != 0 or self._monthly_consolidator_sma.samples != 0:
            raise Exception("Expected monthly consolidator to not have consolidated any data")

        if self._weekly_consolidator_sma.samples != expected_weekly_consolidations:
            raise Exception(f"Expected {expected_weekly_consolidations} samples in the weekly consolidator SMA but found {self._weekly_consolidator_sma.samples}")

        if self._weekly_consolidator_sma.current.time != self._last_weekly_sma_update:
            raise Exception(f"Expected weekly consolidator SMA to have been last updated at {self._last_weekly_sma_update} but was {self._weekly_consolidator_sma.current.time}")

    # on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    def on_data(self, data):
        if not self.portfolio.invested and self._future.has_data:
           self.set_holdings(self._future.symbol, 0.5)
