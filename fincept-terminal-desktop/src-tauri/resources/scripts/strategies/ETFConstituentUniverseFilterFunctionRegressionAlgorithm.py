# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9E29F41C
# Category: ETF
# Description: Tests a custom filter function when creating an ETF constituents universe for SPY
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Tests a custom filter function when creating an ETF constituents universe for SPY
### </summary>
class ETFConstituentUniverseFilterFunctionRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2020, 12, 1)
        self.set_end_date(2021, 1, 31)
        self.set_cash(100000)

        self.filtered = False
        self.securities_changed = False
        self.received_data = False
        self.etf_constituent_data = {}
        self.etf_rebalanced = False
        self.rebalance_count = 0
        self.rebalance_asset_count = 0

        self.universe_settings.resolution = Resolution.HOUR

        self.spy = self.add_equity("SPY", Resolution.HOUR).symbol
        self.aapl = Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)

        self.add_universe(self.universe.etf(self.spy, self.universe_settings, self.filter_etfs))

    def filter_etfs(self, constituents):
        constituents_data = list(constituents)
        constituents_symbols = [i.symbol for i in constituents_data]
        self.etf_constituent_data = {i.symbol: i for i in constituents_data}

        if len(constituents_data) == 0:
            raise Exception(f"Constituents collection is empty on {self.utc_time.strftime('%Y-%m-%d %H:%M:%S.%f')}")
        if self.aapl not in constituents_symbols:
            raise Exception("AAPL is not int he constituents data provided to the algorithm")

        aapl_data = [i for i in constituents_data if i.symbol == self.aapl][0]
        if aapl_data.weight == 0.0:
            raise Exception("AAPL weight is expected to be a non-zero value")

        self.filtered = True
        self.etf_rebalanced = True

        return constituents_symbols

    def on_data(self, data):
        if not self.filtered and len(data.bars) != 0 and self.aapl in data.bars:
            raise Exception("AAPL TradeBar data added to algorithm before constituent universe selection took place")

        if len(data.bars) == 1 and self.spy in data.bars:
            return

        if len(data.bars) != 0 and self.aapl not in data.bars:
            raise Exception(f"Expected AAPL TradeBar data on {self.utc_time.strftime('%Y-%m-%d %H:%M:%S.%f')}")

        self.received_data = True

        if not self.etf_rebalanced:
            return

        for bar in data.bars.values():
            constituent_data = self.etf_constituent_data.get(bar.symbol)
            if constituent_data is not None and constituent_data.weight is not None and constituent_data.weight >= 0.0001:
                # If the weight of the constituent is less than 1%, then it will be set to 1%
                # If the weight of the constituent exceeds more than 5%, then it will be capped to 5%
                # Otherwise, if the weight falls in between, then we use that value.
                bounded_weight = max(0.01, min(constituent_data.weight, 0.05))

                self.set_holdings(bar.symbol, bounded_weight)

                if self.etf_rebalanced:
                    self.rebalance_count += 1

                self.etf_rebalanced = False
                self.rebalance_asset_count += 1


    def on_securities_changed(self, changes):
        if self.filtered and not self.securities_changed and len(changes.added_securities) < 500:
            raise Exception(f"Added SPY S&P 500 ETF to algorithm, but less than 500 equities were loaded (added {len(changes.added_securities)} securities)")

        self.securities_changed = True

    def on_end_of_algorithm(self):
        if self.rebalance_count != 2:
            raise Exception(f"Expected 2 rebalance, instead rebalanced: {self.rebalance_count}")

        if self.rebalance_asset_count != 8:
            raise Exception(f"Invested in {self.rebalance_asset_count} assets (expected 8)")

        if not self.filtered:
            raise Exception("Universe selection was never triggered")

        if not self.securities_changed:
            raise Exception("Security changes never propagated to the algorithm")

        if not self.received_data:
            raise Exception("Data was never loaded for the S&P 500 constituent AAPL")
