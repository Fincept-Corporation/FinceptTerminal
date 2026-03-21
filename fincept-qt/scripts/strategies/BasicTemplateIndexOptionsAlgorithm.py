# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-210273A6
# Category: Options
# Description: Basic Template Index Options Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class BasicTemplateIndexOptionsAlgorithm(QCAlgorithm):
    def initialize(self) -> None:
        self.set_start_date(2021, 1, 4)
        self.set_end_date(2021, 2, 1)
        self.set_cash(1000000)

        self.spx = self.add_index("SPX", Resolution.MINUTE).symbol
        spx_options = self.add_index_option(self.spx, Resolution.MINUTE)
        spx_options.set_filter(lambda x: x.calls_only())

        self.ema_slow = self.ema(self.spx, 80)
        self.ema_fast = self.ema(self.spx, 200)

    def on_data(self, data: Slice) -> None:
        if self.spx not in data.bars or not self.ema_slow.is_ready:
            return

        for chain in data.option_chains.values():
            for contract in chain.contracts.values():
                if self.portfolio.invested:
                    continue

                if (self.ema_fast > self.ema_slow and contract.right == OptionRight.CALL) or \
                    (self.ema_fast < self.ema_slow and contract.right == OptionRight.PUT):

                    self.liquidate(self.invert_option(contract.symbol))
                    self.market_order(contract.symbol, 1)

    def on_end_of_algorithm(self) -> None:
        if self.portfolio[self.spx].total_sale_volume > 0:
            raise Exception("Index is not tradable.")

        if self.portfolio.total_sale_volume == 0:
            raise Exception("Trade volume should be greater than zero by the end of this algorithm")

    def invert_option(self, symbol: Symbol) -> Symbol:
        return Symbol.create_option(
            symbol.underlying,
            symbol.id.market,
            symbol.id.option_style,
            OptionRight.PUT if symbol.id.option_right == OptionRight.CALL else OptionRight.CALL,
            symbol.id.strike_price,
            symbol.id.date
        )
