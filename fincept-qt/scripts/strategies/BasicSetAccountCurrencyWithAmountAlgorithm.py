# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9A26B9BE
# Category: General Strategy
# Description: Basic algorithm using SetAccountCurrency with an amount
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from BasicSetAccountCurrencyAlgorithm import BasicSetAccountCurrencyAlgorithm

### <summary>
### Basic algorithm using SetAccountCurrency with an amount
### </summary>
class BasicSetAccountCurrencyWithAmountAlgorithm(BasicSetAccountCurrencyAlgorithm):
    def set_account_currency_and_amount(self):
        # Before setting any cash or adding a Security call SetAccountCurrency
        self.set_account_currency("EUR", 200000)
