# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C4535414
# Category: Futures
# Description: Algorithm demonstrating and ensuring that Bybit crypto futures brokerage model works as expected
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from cmath import isclose
from AlgorithmImports import *

### <summary>
### Algorithm demonstrating and ensuring that Bybit crypto futures brokerage model works as expected
### </summary>
class BybitCryptoFuturesRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2022, 12, 13)
        self.set_end_date(2022, 12, 13)

        # Set strategy cash (USD)
        self.set_cash(100000)

        self.set_brokerage_model(BrokerageName.BYBIT, AccountType.MARGIN)

        # Translate lines 44-59 to Python:

        self.add_crypto("BTCUSDT", Resolution.MINUTE)

        self.btc_usdt = self.add_crypto_future("BTCUSDT", Resolution.MINUTE)
        self.btc_usd = self.add_crypto_future("BTCUSD", Resolution.MINUTE)

        # create two moving averages
        self.fast = self.ema(self.btc_usdt.symbol, 30, Resolution.MINUTE)
        self.slow = self.ema(self.btc_usdt.symbol, 60, Resolution.MINUTE)

        self.interest_per_symbol = {}
        self.interest_per_symbol[self.btc_usd.symbol] = 0
        self.interest_per_symbol[self.btc_usdt.symbol] = 0

        # the amount of USDT we need to hold to trade 'BTCUSDT'
        self.btc_usdt.quote_currency.set_amount(200)
        # the amount of BTC we need to hold to trade 'BTCUSD'
        self.btc_usd.base_currency.set_amount(0.005)

    def on_data(self, data):
        interest_rates = data.get[MarginInterestRate]()
        for interest_rate in interest_rates:
            self.interest_per_symbol[interest_rate.key] += 1

            cached_interest_rate = self.securities[interest_rate.key].cache.get_data[MarginInterestRate]()
            if cached_interest_rate != interest_rate.value:
                raise Exception(f"Unexpected cached margin interest rate for {interest_rate.key}!")

        if not self.slow.is_ready:
            return

        if self.fast > self.slow:
            if not self.portfolio.invested and self.transactions.orders_count == 0:
                ticket = self.buy(self.btc_usd.symbol, 1000)
                if ticket.status != OrderStatus.INVALID:
                    raise Exception(f"Unexpected valid order {ticket}, should fail due to margin not sufficient")

                self.buy(self.btc_usd.symbol, 100)

                margin_used = self.portfolio.total_margin_used
                btc_usd_holdings = self.btc_usd.holdings

                # Coin futures value is 100 USD
                holdings_value_btc_usd = 100
                if abs(btc_usd_holdings.total_sale_volume - holdings_value_btc_usd) > 1:
                    raise Exception(f"Unexpected TotalSaleVolume {btc_usd_holdings.total_sale_volume}")
                if abs(btc_usd_holdings.absolute_holdings_cost - holdings_value_btc_usd) > 1:
                    raise Exception(f"Unexpected holdings cost {btc_usd_holdings.holdings_cost}")
                # margin used is based on the maintenance rate
                if (abs(btc_usd_holdings.absolute_holdings_cost * 0.05 - margin_used) > 1 or
                    not isclose(self.btc_usd.buying_power_model.get_maintenance_margin(MaintenanceMarginParameters.for_current_holdings(self.btc_usd)).value, margin_used)):
                    raise Exception(f"Unexpected margin used {margin_used}")

                self.buy(self.btc_usdt.symbol, 0.01)

                margin_used = self.portfolio.total_margin_used - margin_used
                btc_usdt_holdings = self.btc_usdt.holdings

                # USDT futures value is based on it's price
                holdings_value_usdt = self.btc_usdt.price * self.btc_usdt.symbol_properties.contract_multiplier * 0.01

                if abs(btc_usdt_holdings.total_sale_volume - holdings_value_usdt) > 1:
                    raise Exception(f"Unexpected TotalSaleVolume {btc_usdt_holdings.total_sale_volume}")
                if abs(btc_usdt_holdings.absolute_holdings_cost - holdings_value_usdt) > 1:
                    raise Exception(f"Unexpected holdings cost {btc_usdt_holdings.holdings_cost}")
                if (abs(btc_usdt_holdings.absolute_holdings_cost * 0.05 - margin_used) > 1 or
                    not isclose(self.btc_usdt.buying_power_model.get_maintenance_margin(MaintenanceMarginParameters.for_current_holdings(self.btc_usdt)).value, margin_used)):
                    raise Exception(f"Unexpected margin used {margin_used}")

                 # position just opened should be just spread here
                unrealized_profit = self.portfolio.total_unrealized_profit
                if (5 - abs(unrealized_profit)) < 0:
                    raise Exception(f"Unexpected TotalUnrealizedProfit {self.portfolio.total_unrealized_profit}")

                if self.portfolio.total_profit != 0:
                    raise Exception(f"Unexpected TotalProfit {self.portfolio.total_profit}")
        # let's revert our position
        elif self.transactions.orders_count == 3:
            self.sell(self.btc_usd.symbol, 300)

            btc_usd_holdings = self.btc_usd.holdings
            if abs(btc_usd_holdings.absolute_holdings_cost - 100 * 2) > 1:
                raise Exception(f"Unexpected holdings cost {btc_usd_holdings.holdings_cost}")

            self.sell(self.btc_usdt.symbol, 0.03)

            # USDT futures value is based on it's price
            holdings_value_usdt = self.btc_usdt.price * self.btc_usdt.symbol_properties.contract_multiplier * 0.02
            if abs(self.btc_usdt.holdings.absolute_holdings_cost - holdings_value_usdt) > 1:
                raise Exception(f"Unexpected holdings cost {self.btc_usdt.holdings.holdings_cost}")

            # position just opened should be just spread here
            profit = self.portfolio.total_unrealized_profit
            if (5 - abs(profit)) < 0:
                raise Exception(f"Unexpected TotalUnrealizedProfit {self.portfolio.total_unrealized_profit}")
            # we barely did any difference on the previous trade
            if (5 - abs(self.portfolio.total_profit)) < 0:
                raise Exception(f"Unexpected TotalProfit {self.portfolio.total_profit}")

    def on_order_event(self, order_event):
        self.debug("{} {}".format(self.time, order_event.to_string()))

    def on_end_of_algorithm(self):
        self.log(f"{self.time} - TotalPortfolioValue: {self.portfolio.total_portfolio_value}")
        self.log(f"{self.time} - CashBook: {self.portfolio.cash_book}")

        if any(x == 0 for x in self.interest_per_symbol.values()):
            raise Exception("Expected interest rate data for all symbols")
