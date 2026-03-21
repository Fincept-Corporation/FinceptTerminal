# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-8205EE4A
# Category: Crypto
# Description: Algorithm demonstrating and ensuring that Bybit crypto brokerage model works as expected
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Algorithm demonstrating and ensuring that Bybit crypto brokerage model works as expected
### </summary>
class BybitCryptoRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2022, 12, 13)
        self.set_end_date(2022, 12, 13)

        # Set account currency (USDT)
        self.set_account_currency("USDT")

        # Set strategy cash (USD)
        self.set_cash(100000)

        # Add some coin as initial holdings
        # When connected to a real brokerage, the amount specified in SetCash
        # will be replaced with the amount in your actual account.
        self.set_cash("BTC", 1)

        self.set_brokerage_model(BrokerageName.BYBIT, AccountType.CASH)

        self.btc_usdt = self.add_crypto("BTCUSDT").symbol

        # create two moving averages
        self.fast = self.ema(self.btc_usdt, 30, Resolution.MINUTE)
        self.slow = self.ema(self.btc_usdt, 60, Resolution.MINUTE)

        self.liquidated = False

    def on_data(self, data):
        if self.portfolio.cash_book["USDT"].conversion_rate == 0 or self.portfolio.cash_book["BTC"].conversion_rate == 0:
            self.log(f"USDT conversion rate: {self.portfolio.cash_book['USDT'].conversion_rate}")
            self.log(f"BTC conversion rate: {self.portfolio.cash_book['BTC'].conversion_rate}")

            raise Exception("Conversion rate is 0")

        if not self.slow.is_ready:
            return

        btc_amount = self.portfolio.cash_book["BTC"].amount
        if self.fast > self.slow:
            if btc_amount == 1 and not self.liquidated:
                self.buy(self.btc_usdt, 1)
        else:
            if btc_amount > 1:
                self.liquidate(self.btc_usdt)
                self.liquidated = True
            elif btc_amount > 0 and self.liquidated and len(self.transactions.get_open_orders()) == 0:
                # Place a limit order to sell our initial BTC holdings at 1% above the current price
                limit_price = round(self.securities[self.btc_usdt].price * 1.01, 2)
                self.limit_order(self.btc_usdt, -btc_amount, limit_price)

    def on_order_event(self, order_event):
        self.debug("{} {}".format(self.time, order_event.to_string()))

    def on_end_of_algorithm(self):
        self.log(f"{self.time} - TotalPortfolioValue: {self.portfolio.total_portfolio_value}")
        self.log(f"{self.time} - CashBook: {self.portfolio.cash_book}")

        btc_amount = self.portfolio.cash_book["BTC"].amount
        if btc_amount > 0:
            raise Exception(f"BTC holdings should be zero at the end of the algorithm, but was {btc_amount}")
