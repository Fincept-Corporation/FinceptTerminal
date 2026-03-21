# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-88C8DA85
# Category: Index
# Description: Basic Template Tradable Index Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from BasicTemplateIndexAlgorithm import BasicTemplateIndexAlgorithm

class BasicTemplateTradableIndexAlgorithm(BasicTemplateIndexAlgorithm):
    ticket = None
    def initialize(self) -> None:
        super().initialize()
        self.securities[self.spx].is_tradable = True
        
    def on_data(self, data: Slice):
        super().on_data(data)
        if not self.ticket:
            self.ticket = self.market_order(self.spx, 1)

    def on_end_of_algorithm(self) -> None:
        if self.ticket.status != OrderStatus.FILLED:
            raise Exception("Index is tradable.")
