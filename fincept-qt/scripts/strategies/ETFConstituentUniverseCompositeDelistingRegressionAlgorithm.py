# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-2A9A1A33
# Category: ETF
# Description: Tests the delisting of the composite Symbol (ETF symbol) and the removal of the universe and the symbol from the algo...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Tests the delisting of the composite Symbol (ETF symbol) and the removal of
### the universe and the symbol from the algorithm.
### </summary>
class ETFConstituentUniverseCompositeDelistingRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2020, 12, 1)
        self.set_end_date(2021, 1, 31)
        self.set_cash(100000)

        self.universe_symbol_count = 0
        self.universe_selection_done = False
        self.universe_added = False
        self.universe_removed = False

        self.universe_settings.resolution = Resolution.HOUR
        self.delisting_date = date(2021, 1, 21)

        self.aapl = self.add_equity("AAPL", Resolution.HOUR).symbol
        self.gdvd = self.add_equity("GDVD", Resolution.HOUR).symbol

        self.add_universe(self.universe.etf(self.gdvd, self.universe_settings, self.filter_etfs))

    def filter_etfs(self, constituents):
        self.universe_selection_done = True

        if self.utc_time.date() > self.delisting_date:
            raise Exception(f"Performing constituent universe selection on {self.utc_time.strftime('%Y-%m-%d %H:%M:%S.%f')} after composite ETF has been delisted")

        constituent_symbols = [i.symbol for i in constituents]
        self.universe_symbol_count = len(set(constituent_symbols))

        return constituent_symbols

    def on_data(self, data):
        if self.utc_time.date() > self.delisting_date and any([i != self.aapl for i in data.keys()]):
            raise Exception("Received unexpected slice in OnData(...) after universe was deselected")

        if not self.portfolio.invested:
            self.set_holdings(self.aapl, 0.5)

    def on_securities_changed(self, changes):
        if len(changes.added_securities) != 0 and self.utc_time.date() > self.delisting_date:
            raise Exception("New securities added after ETF constituents were delisted")

        # Since we added the etf subscription it will get delisted and send us a removal event
        expected_changes_count = self.universe_symbol_count + 1

        if self.universe_selection_done:
            # "_universe_symbol_count + 1" because selection is done right away,
            # so AddedSecurities includes all ETF constituents (including APPL) plus GDVD
            self.universe_added = self.universe_added or len(changes.added_securities) == expected_changes_count

        # TODO: shouldn't be sending AAPL as a removed security since it was added by another universe
        self.universe_removed = self.universe_removed or (
            len(changes.removed_securities) == expected_changes_count and
            self.utc_time.date() >= self.delisting_date and
            self.utc_time.date() < self.end_date.date())

    def on_end_of_algorithm(self):
        if not self.universe_added:
            raise Exception("ETF constituent universe was never added to the algorithm")
        if not self.universe_removed:
            raise Exception("ETF constituent universe was not removed from the algorithm after delisting")
        if len(self.active_securities) > 2:
            raise Exception(f"Expected less than 2 securities after algorithm ended, found {len(self.securities)}")
