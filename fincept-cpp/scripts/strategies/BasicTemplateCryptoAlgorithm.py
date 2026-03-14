# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C45FB406
# Category: Crypto
# Description: The demonstration algorithm shows some of the most common order methods when working with Crypto assets
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### The demonstration algorithm shows some of the most common order methods when working with Crypto assets.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="trading and orders" />
class BasicTemplateCryptoAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2018, 4, 4)  #Set Start Date
        self.set_end_date(2018, 4, 4)    #Set End Date

        # Although typically real brokerages as GDAX only support a single account currency,
        # here we add both USD and EUR to demonstrate how to handle non-USD account currencies.
        # Set Strategy Cash (USD)
        self.set_cash(10000)

        # Set Strategy Cash (EUR)
        # EUR/USD conversion rate will be updated dynamically
        self.set_cash("EUR", 10000)

        # Add some coins as initial holdings
        # When connected to a real brokerage, the amount specified in SetCash
        # will be replaced with the amount in your actual account.
        self.set_cash("BTC", 1)
        self.set_cash("ETH", 5)

        self.set_brokerage_model(BrokerageName.GDAX, AccountType.CASH)

        # You can uncomment the following lines when live trading with GDAX,
        # to ensure limit orders will only be posted to the order book and never executed as a taker (incurring fees).
        # Please note this statement has no effect in backtesting or paper trading.
        # self.default_order_properties = GDAXOrderProperties()
        # self.default_order_properties.post_only = True

        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_crypto("BTCUSD", Resolution.MINUTE)
        self.add_crypto("ETHUSD", Resolution.MINUTE)
        self.add_crypto("BTCEUR", Resolution.MINUTE)
        symbol = self.add_crypto("LTCUSD", Resolution.MINUTE).symbol

        # create two moving averages
        self.fast = self.ema(symbol, 30, Resolution.MINUTE)
        self.slow = self.ema(symbol, 60, Resolution.MINUTE)

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''

        # Note: all limit orders in this algorithm will be paying taker fees,
        # they shouldn't, but they do (for now) because of this issue:
        # https://github.com/QuantConnect/Lean/issues/1852

        if self.time.hour == 1 and self.time.minute == 0:
            # Sell all ETH holdings with a limit order at 1% above the current price
            limit_price = round(self.securities["ETHUSD"].price * 1.01, 2)
            quantity = self.portfolio.cash_book["ETH"].amount
            self.limit_order("ETHUSD", -quantity, limit_price)

        elif self.time.hour == 2 and self.time.minute == 0:
            # Submit a buy limit order for BTC at 5% below the current price
            usd_total = self.portfolio.cash_book["USD"].amount
            limit_price = round(self.securities["BTCUSD"].price * 0.95, 2)
            # use only half of our total USD
            quantity = usd_total * 0.5 / limit_price
            self.limit_order("BTCUSD", quantity, limit_price)

        elif self.time.hour == 2 and self.time.minute == 1:
            # Get current USD available, subtracting amount reserved for buy open orders
            usd_total = self.portfolio.cash_book["USD"].amount
            usd_reserved = sum(x.quantity * x.limit_price for x
                in [x for x in self.transactions.get_open_orders()
                    if x.direction == OrderDirection.BUY
                        and x.type == OrderType.LIMIT
                        and (x.symbol.value == "BTCUSD" or x.symbol.value == "ETHUSD")])
            usd_available = usd_total - usd_reserved
            self.debug("usd_available: {}".format(usd_available))

            # Submit a marketable buy limit order for ETH at 1% above the current price
            limit_price = round(self.securities["ETHUSD"].price * 1.01, 2)

            # use all of our available USD
            quantity = usd_available / limit_price

            # this order will be rejected (for now) because of this issue:
            # https://github.com/QuantConnect/Lean/issues/1852
            self.limit_order("ETHUSD", quantity, limit_price)

            # use only half of our available USD
            quantity = usd_available * 0.5 / limit_price
            self.limit_order("ETHUSD", quantity, limit_price)

        elif self.time.hour == 11 and self.time.minute == 0:
            # Liquidate our BTC holdings (including the initial holding)
            self.set_holdings("BTCUSD", 0)

        elif self.time.hour == 12 and self.time.minute == 0:
            # Submit a market buy order for 1 BTC using EUR
            self.buy("BTCEUR", 1)

            # Submit a sell limit order at 10% above market price
            limit_price = round(self.securities["BTCEUR"].price * 1.1, 2)
            self.limit_order("BTCEUR", -1, limit_price)

        elif self.time.hour == 13 and self.time.minute == 0:
            # Cancel the limit order if not filled
            self.transactions.cancel_open_orders("BTCEUR")

        elif self.time.hour > 13:
            # To include any initial holdings, we read the LTC amount from the cashbook
            # instead of using Portfolio["LTCUSD"].quantity

            if self.fast > self.slow:
                if self.portfolio.cash_book["LTC"].amount == 0:
                    self.buy("LTCUSD", 10)
            else:
                if self.portfolio.cash_book["LTC"].amount > 0:
                    # The following two statements currently behave differently if we have initial holdings:
                    # https://github.com/QuantConnect/Lean/issues/1860

                    self.liquidate("LTCUSD")
                    # self.set_holdings("LTCUSD", 0)

    def on_order_event(self, order_event):
        self.debug("{} {}".format(self.time, order_event.to_string()))

    def on_end_of_algorithm(self):
        self.log("{} - TotalPortfolioValue: {}".format(self.time, self.portfolio.total_portfolio_value))
        self.log("{} - CashBook: {}".format(self.time, self.portfolio.cash_book))
