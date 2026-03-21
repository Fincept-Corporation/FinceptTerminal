# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E96624E8
# Category: Futures
# Description: Hourly regression algorithm trading ADAUSDT binance futures long and short asserting the behavior
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Hourly regression algorithm trading ADAUSDT binance futures long and short asserting the behavior
### </summary>
class BasicTemplateCryptoFutureHourlyAlgorithm(QCAlgorithm):
    # <summary>
    # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    # </summary>

    def initialize(self):
        self.set_start_date(2022, 12, 13)
        self.set_end_date(2022, 12, 13)

        self.set_time_zone(TimeZones.UTC)

        try:
            self.set_brokerage_model(BrokerageName.BINANCE_COIN_FUTURES, AccountType.CASH)
        except:
            # expected, we don't allow cash account type
            pass

        self.set_brokerage_model(BrokerageName.BINANCE_COIN_FUTURES, AccountType.MARGIN)

        self.ada_usdt = self.add_crypto_future("ADAUSDT", Resolution.HOUR)

        self.fast = self.ema(self.ada_usdt.symbol, 3, Resolution.HOUR)
        self.slow = self.ema(self.ada_usdt.symbol, 6, Resolution.HOUR)

        self.interest_per_symbol = {self.ada_usdt.symbol: 0}

        # Default USD cash, set 1M but it wont be used
        self.set_cash(1000000)

        # the amount of USDT we need to hold to trade 'ADAUSDT'
        self.ada_usdt.quote_currency.set_amount(200)

    # <summary>
    # OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    # </summary>
    # <param name="data">Slice object keyed by symbol containing the stock data</param>
    def on_data(self, slice):
        interest_rates = slice.get(MarginInterestRate);
        for interest_rate in interest_rates:
            self.interest_per_symbol[interest_rate.key] += 1
            self.cached_interest_rate = self.securities[interest_rate.key].cache.get_data[MarginInterestRate]()
            if self.cached_interest_rate != interest_rate.value:
                raise Exception(f"Unexpected cached margin interest rate for {interest_rate.key}!")

        if self.fast > self.slow:
            if self.portfolio.invested == False and self.transactions.orders_count == 0:
                self.ticket = self.buy(self.ada_usdt.symbol, 100000)
                if self.ticket.status != OrderStatus.INVALID:
                    raise Exception(f"Unexpected valid order {self.ticket}, should fail due to margin not sufficient")

                self.buy(self.ada_usdt.symbol, 1000)

                self.margin_used = self.portfolio.total_margin_used

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
            # let's revert our position and double
            if self.time.hour > 10 and self.transactions.orders_count == 2:
                self.sell(self.ada_usdt.symbol, 3000)

                self.ada_usdt_holdings = self.ada_usdt.holdings

                # USDT/BUSD futures value is based on it's price
                self.holdings_value_usdt = self.ada_usdt.price * self.ada_usdt.symbol_properties.contract_multiplier * 2000

                if abs(self.ada_usdt_holdings.absolute_holdings_cost - self.holdings_value_usdt) > 1:
                    raise Exception(f"Unexpected holdings cost {self.ada_usdt_holdings.holdings_cost}")

                # position just opened should be just spread here
                self.profit = self.portfolio.total_unrealized_profit
                if (5 - abs(self.profit)) < 0:
                    raise Exception(f"Unexpected TotalUnrealizedProfit {self.portfolio.total_unrealized_profit}")

                # we barely did any difference on the previous trade
                if (5 - abs(self.portfolio.total_profit)) < 0:
                    raise Exception(f"Unexpected TotalProfit {self.portfolio.total_profit}")

            if self.time.hour >= 22 and self.transactions.orders_count == 3:
                self.liquidate()

    def on_end_of_algorithm(self):
        if self.interest_per_symbol[self.ada_usdt.symbol] != 1:
                raise Exception(f"Unexpected interest rate count {self.interest_per_symbol[self.ada_usdt.symbol]}")

    def on_order_event(self, order_event):
        self.debug("{0} {1}".format(self.time, order_event))
