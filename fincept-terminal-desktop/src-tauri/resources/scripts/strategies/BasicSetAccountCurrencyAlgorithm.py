# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-07DF2AB7
# Category: General Strategy
# Description: Basic algorithm using SetAccountCurrency
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Basic algorithm using SetAccountCurrency
### </summary>
class BasicSetAccountCurrencyAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2018, 4, 4)  #Set Start Date
        self.set_end_date(2018, 4, 4)    #Set End Date
        self.set_brokerage_model(BrokerageName.GDAX, AccountType.CASH)
        self.set_account_currency_and_amount()

        self._btc_eur = self.add_crypto("BTCEUR").symbol

    def set_account_currency_and_amount(self):
        # Before setting any cash or adding a Security call SetAccountCurrency
        self.set_account_currency("EUR")
        self.set_cash(100000)           #Set Strategy Cash

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            self.set_holdings(self._btc_eur, 1)
