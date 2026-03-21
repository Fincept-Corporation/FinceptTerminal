# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A17DA593
# Category: Index
# Description: Basic Template Index Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicTemplateIndexAlgorithm(QCAlgorithm):
    def initialize(self) -> None:
        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 1, 18)
        self.set_cash(1000000)

        # Use indicator for signal; but it cannot be traded
        self.spx = self.add_index("SPX", Resolution.MINUTE).symbol

        # Trade on SPX ITM calls
        self.spx_option = Symbol.create_option(
            self.spx,
            Market.USA,
            OptionStyle.EUROPEAN,
            OptionRight.CALL,
            3200,
            datetime(2021, 1, 15)
        )

        self.add_index_option_contract(self.spx_option, Resolution.MINUTE)

        self.ema_slow = self.ema(self.spx, 80)
        self.ema_fast = self.ema(self.spx, 200)

    def on_data(self, data: Slice):
        if self.spx not in data.bars or self.spx_option not in data.bars:
            return

        if not self.ema_slow.is_ready:
            return

        if self.ema_fast > self.ema_slow:
            self.set_holdings(self.spx_option, 1)
        else:
            self.liquidate()

    def on_end_of_algorithm(self) -> None:
        if self.portfolio[self.spx].total_sale_volume > 0:
            raise Exception("Index is not tradable.")
