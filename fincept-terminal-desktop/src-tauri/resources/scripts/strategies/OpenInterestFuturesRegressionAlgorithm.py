# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-370B9F0E
# Category: Futures
# Description: Futures framework algorithm that uses open interest to select the active contract
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Futures framework algorithm that uses open interest to select the active contract.
### </summary>
### <meta name="tag" content="regression test" />
### <meta name="tag" content="futures" />
### <meta name="tag" content="using data" />
### <meta name="tag" content="filter selection" />
class OpenInterestFuturesRegressionAlgorithm(QCAlgorithm):

    expected_expiry_dates = {datetime(2013, 12, 27), datetime(2014,2,26)}

    def initialize(self):
        self.universe_settings.resolution = Resolution.TICK
        self.set_start_date(2013,10,8)
        self.set_end_date(2013,10,11)
        self.set_cash(10000000)

        # set framework models
        universe = OpenInterestFutureUniverseSelectionModel(self, lambda date_time: [Symbol.create(Futures.Metals.GOLD, SecurityType.FUTURE, Market.COMEX)], None, len(self.expected_expiry_dates))
        self.set_universe_selection(universe)
    
    def on_data(self,data):
        if self.transactions.orders_count == 0 and data.has_data:
            matched = list(filter(lambda s: not (s.id.date in self.expected_expiry_dates) and not s.is_canonical(), data.keys()))
            if len(matched) != 0:
                raise Exception(f"{len(matched)}/{len(slice.keys)} were unexpected expiry date(s): " + ", ".join(list(map(lambda x: x.id.date, matched))))

            for symbol in data.keys():
                self.market_order(symbol, 1)
        elif any(p.value.invested for p in self.portfolio):
            self.liquidate()
