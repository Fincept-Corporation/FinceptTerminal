# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-4F8A816B
# Category: Risk Management
# Description: Example algorithm of using RiskParityPortfolioConstructionModel
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Portfolio.RiskParityPortfolioConstructionModel import *

class RiskParityPortfolioAlgorithm(QCAlgorithm):
    '''Example algorithm of using RiskParityPortfolioConstructionModel'''

    def initialize(self):
        self.set_start_date(2021, 2, 21)  # Set Start Date
        self.set_end_date(2021, 3, 30)
        self.set_cash(100000)  # Set Strategy Cash
        self.set_security_initializer(lambda security: security.set_market_price(self.get_last_known_price(security)))

        self.add_equity("SPY", Resolution.DAILY)
        self.add_equity("AAPL", Resolution.DAILY)

        self.add_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1)))
        self.set_portfolio_construction(RiskParityPortfolioConstructionModel())
