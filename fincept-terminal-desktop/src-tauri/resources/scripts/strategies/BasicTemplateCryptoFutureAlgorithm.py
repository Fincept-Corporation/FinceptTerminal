# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-F09974A0
# Category: Futures
# Description: Minute resolution regression algorithm trading Coin and USDT binance futures long and short asserting the behavior
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Minute resolution regression algorithm trading Coin and USDT binance futures long and short asserting the behavior
### </summary>
class BasicTemplateCryptoFutureAlgorithm(QCAlgorithm):
    # <summary>
    # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    # </summary>

    def initialize(self):
        self.set_start_date(2022, 12, 13)
        self.set_end_date(2022, 12, 13)

        self.set_time_zone(TimeZones.UTC)

        try:
            self.set_brokerage_model(BrokerageName.BINANCE_FUTURES, AccountType.CASH)
        except:
            # expected, we don't allow cash account type
            pass

        self.set_brokerage_model(BrokerageName.BINANCE_FUTURES, AccountType.MARGIN)

        self.btc_usd = self.add_crypto_future("BTCUSD")
        self.ada_usdt = self.add_crypto_future("ADAUSDT")

        self.fast = self.ema(self.btc_usd.symbol, 30, Resolution.MINUTE)
        self.slow = self.ema(self.btc_usd.symbol, 60, Resolution.MINUTE)

        self.interest_per_symbol = {self.btc_usd.symbol: 0, self.ada_usdt.symbol: 0}

        self.set_cash(1000000)

        # the amount of BTC we need to hold to trade 'BTCUSD'
        self.btc_usd.base_currency.set_amount(0.005)
        # the amount of USDT we need to hold to trade 'ADAUSDT'
        self.ada_usdt.quote_currency.set_amount(200)

    # <summary>
    # OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    # </summary>
    # <param name="data">Slice object keyed by symbol containing the stock data</param>
    def on_data(self, slice):
        interest_rates = slice.Get(MarginInterestRate)
        for interest_rate in interest_rates:
            self.interest_per_symbol[interest_rate.key] += 1
            self.cached_interest_rate = self.securities[interest_rate.key].cache.get_data[MarginInterestRate]()
            if self.cached_interest_rate != interest_rate.value:
                raise Exception(f"Unexpected cached margin interest rate for {interest_rate.key}!")
            
        if self.fast > self.slow:
            if self.portfolio.invested == False and self.transactions.orders_count == 0:
                self.ticket = self.buy(self.btc_usd.symbol, 50)
                if self.ticket.status != OrderStatus.INVALID:
                    raise Exception(f"Unexpected valid order {self.ticket}, should fail due to margin not sufficient")
                
                self.buy(self.btc_usd.symbol, 1)

                self.margin_used = self.portfolio.total_margin_used
                self.btc_usd_holdings = self.btc_usd.holdings
                # Coin futures value is 100 USD
                self.holdings_value_btc_usd = 100

                if abs(self.btc_usd_holdings.total_sale_volume - self.holdings_value_btc_usd) > 1:
                    raise Exception(f"Unexpected TotalSaleVolume {self.btc_usd_holdings.total_sale_volume}")
                
                if abs(self.btc_usd_holdings.absolute_holdings_cost - self.holdings_value_btc_usd) > 1:
                    raise Exception(f"Unexpected holdings cost {self.btc_usd_holdings.holdings_cost}")
                
                # margin used is based on the maintenance rate
                if (abs(self.btc_usd_holdings.absolute_holdings_cost * 0.05 - self.margin_used) > 1) or (BuyingPowerModelExtensions.get_maintenance_margin(self.btc_usd.buying_power_model, self.btc_usd) != self.margin_used):
                    raise Exception(f"Unexpected margin used {self.margin_used}")
                
                self.buy(self.ada_usdt.symbol, 1000)

                self.margin_used = self.portfolio.total_margin_used - self.margin_used
                self.ada_usdt_holdings = self.ada_usdt.holdings

                # USDT/BUSD futures value is based on it's price
                self.holdings_value_usdt = self.ada_usdt.price * self.ada_usdt.symbol_properties.contract_multiplier * 1000

                if abs(self.ada_usdt_holdings.total_sale_volume - self.holdings_value_usdt) > 1:
                    raise Exception(f"Unexpected TotalSaleVolume {self.ada_usdt_holdings.total_sale_volume}")
                
                if abs(self.ada_usdt_holdings.absolute_holdings_cost - self.holdings_value_usdt) > 1:
                    raise Exception(f"Unexpected holdings cost {self.ada_usdt_holdings.holdings_cost}")
                
                if (abs(self.ada_usdt_holdings.absolute_holdings_cost * 0.05 - self.margin_used) > 1) or (BuyingPowerModelExtensions.get_maintenance_margin(self.ada_usdt.buying_power_model, self.ada_usdt) != self.margin_used):
                    raise Exception(f"Unexpected margin used {self.margin_used}")
                
                # position just opened should be just spread here
                self.profit = self.portfolio.total_unrealized_profit
                
                if (5 - abs(self.profit)) < 0:
                    raise Exception(f"Unexpected TotalUnrealizedProfit {self.portfolio.total_unrealized_profit}")

                if (self.portfolio.total_profit != 0):
                    raise Exception(f"Unexpected TotalProfit {self.portfolio.total_profit}")
                
        else:
            if self.time.hour > 10 and self.transactions.orders_count == 3:
                self.sell(self.btc_usd.symbol, 3)
                self.btc_usd_holdings = self.btc_usd.holdings
                if abs(self.btc_usd_holdings.absolute_holdings_cost - 100 * 2) > 1:
                    raise Exception(f"Unexpected holdings cost {self.btc_usd_holdings.holdings_cost}")

                self.sell(self.ada_usdt.symbol, 3000)
                ada_usdt_holdings = self.ada_usdt.holdings

                # USDT/BUSD futures value is based on it's price
                holdings_value_usdt = self.ada_usdt.price * self.ada_usdt.symbol_properties.contract_multiplier * 2000

                if abs(ada_usdt_holdings.absolute_holdings_cost - holdings_value_usdt) > 1:
                    raise Exception(f"Unexpected holdings cost {ada_usdt_holdings.holdings_cost}")

                # position just opened should be just spread here
                profit = self.portfolio.total_unrealized_profit
                if (5 - abs(profit)) < 0:
                    raise Exception(f"Unexpected TotalUnrealizedProfit {self.portfolio.total_unrealized_profit}")
                # we barely did any difference on the previous trade
                if (5 - abs(self.portfolio.total_profit)) < 0:
                    raise Exception(f"Unexpected TotalProfit {self.portfolio.total_profit}")

                                            
    def on_end_of_algorithm(self):
        if self.interest_per_symbol[self.ada_usdt.symbol] != 1:
                raise Exception(f"Unexpected interest rate count {self.interest_per_symbol[self.ada_usdt.symbol]}")
        if self.interest_per_symbol[self.btc_usd.symbol] != 3:
                raise Exception(f"Unexpected interest rate count {self.interest_per_symbol[self.btc_usd.symbol]}")

    def on_order_event(self, order_event):
        self.debug("{0} {1}".format(self.time, order_event))
