# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-265D788C
# Category: Futures
# Description: This example demonstrates how to get access to futures history for a given root symbol. It also shows how you can pre...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This example demonstrates how to get access to futures history for a given root symbol.
### It also shows how you can prefilter contracts easily based on expirations, and inspect the futures
### chain to pick a specific contract to trade.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="history and warm up" />
### <meta name="tag" content="history" />
### <meta name="tag" content="futures" />
class BasicTemplateFuturesHistoryAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 8)
        self.set_end_date(2013, 10, 9)
        self.set_cash(1000000)

        extended_market_hours = self.get_extended_market_hours()

        # Subscribe and set our expiry filter for the futures chain
        # find the front contract expiring no earlier than in 90 days
        future_es = self.add_future(Futures.Indices.SP_500_E_MINI, Resolution.MINUTE, extended_market_hours=extended_market_hours)
        future_es.set_filter(timedelta(0), timedelta(182))

        future_gc = self.add_future(Futures.Metals.GOLD, Resolution.MINUTE, extended_market_hours=extended_market_hours)
        future_gc.set_filter(timedelta(0), timedelta(182))

        self.set_benchmark(lambda x: 1000000)

        self.schedule.on(self.date_rules.every_day(), self.time_rules.every(timedelta(hours=1)), self.make_history_call)
        self._success_count = 0

    def make_history_call(self):
        history = self.history(self.securities.keys(), 10, Resolution.MINUTE)
        if len(history) < 10:
            raise Exception(f'Empty history at {self.time}')
        self._success_count += 1

    def on_end_of_algorithm(self):
        if self._success_count < self.get_expected_history_call_count():
            raise Exception(f'Scheduled Event did not assert history call as many times as expected: {self._success_count}/49')

    def on_data(self,slice):
        if self.portfolio.invested: return
        for chain in slice.future_chains:
            for contract in chain.value:
                self.log(f'{contract.symbol.value},' +
                         f'Bid={contract.bid_price} ' +
                         f'Ask={contract.ask_price} ' +
                         f'Last={contract.last_price} ' +
                         f'OI={contract.open_interest}')

    def on_securities_changed(self, changes):
        for change in changes.added_securities:
            history = self.history(change.symbol, 10, Resolution.MINUTE).sort_index(level='time', ascending=False)[:3]

            for index, row in history.iterrows():
                self.log(f'History: {index[1]} : {index[2]:%m/%d/%Y %I:%M:%S %p} > {row.close}')

    def on_order_event(self, order_event):
        # Order fill event handler. On an order fill update the resulting information is passed to this method.
        # Order event details containing details of the events
        self.log(f'{order_event}')

    def get_extended_market_hours(self):
        return False

    def get_expected_history_call_count(self):
        return 42
