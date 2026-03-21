# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-98AE653C
# Category: Portfolio Management
# Description: Example algorithm of using MeanReversionPortfolioConstructionModel
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from Portfolio.MeanReversionPortfolioConstructionModel import *

class MeanReversionPortfolioAlgorithm(QCAlgorithm):
    '''Example algorithm of using MeanReversionPortfolioConstructionModel'''

    def initialize(self):
        # Set starting date, cash and ending date of the backtest
        self.set_start_date(2020, 9, 1)
        self.set_end_date(2021, 2, 28)
        self.set_cash(100000)

        self.set_security_initializer(lambda security: security.set_market_price(self.get_last_known_price(security)))
        
        # Subscribe to data of the selected stocks
        self._symbols = [self.add_equity(ticker, Resolution.DAILY).symbol for ticker in ["SPY", "AAPL"]]

        self.add_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1)))
        self.set_portfolio_construction(MeanReversionPortfolioConstructionModel())
