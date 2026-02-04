# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9147ECE5
# Category: Regression Test
# Description: Regression test to demonstrate setting custom Symbol Properties and Market Hours for a custom data import
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression test to demonstrate setting custom Symbol Properties and Market Hours for a custom data import
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="importing data" />
### <meta name="tag" content="custom data" />
### <meta name="tag" content="crypto" />
### <meta name="tag" content="regression test" />
class CustomDataPropertiesRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2020, 1, 5)   # Set Start Date
        self.set_end_date(2020, 1, 10)     # Set End Date
        self.set_cash(100000)           # Set Strategy Cash

        # Define our custom data properties and exchange hours
        self.ticker = 'BTC'
        properties = SymbolProperties("Bitcoin", "USD", 1, 0.01, 0.01, self.ticker)
        exchange_hours = SecurityExchangeHours.always_open(TimeZones.NEW_YORK)

        # Add the custom data to our algorithm with our custom properties and exchange hours
        self.bitcoin = self.add_data(Bitcoin, self.ticker, properties, exchange_hours, leverage=1, fill_forward=False)

        # Verify our symbol properties were changed and loaded into this security
        if self.bitcoin.symbol_properties != properties :
            raise Exception("Failed to set and retrieve custom SymbolProperties for BTC")

        # Verify our exchange hours were changed and loaded into this security
        if self.bitcoin.exchange.hours != exchange_hours :
            raise Exception("Failed to set and retrieve custom ExchangeHours for BTC")

        # For regression purposes on AddData overloads, this call is simply to ensure Lean can accept this
        # with default params and is not routed to a breaking function.
        self.add_data(Bitcoin, "BTCUSD")


    def on_data(self, data):
        if not self.portfolio.invested:
            if data['BTC'].close != 0 :
                self.order('BTC', self.portfolio.margin_remaining/abs(data['BTC'].close + 1))

    def on_end_of_algorithm(self):
        #Reset our Symbol property value, for testing purposes.
        self.symbol_properties_database.set_entry(Market.USA, self.market_hours_database.get_database_symbol_key(self.bitcoin.symbol), SecurityType.BASE,
            SymbolProperties.get_default("USD"))



class Bitcoin(PythonData):
    '''Custom Data Type: Bitcoin data from Quandl - http://www.quandl.com/help/api-for-bitcoin-data'''

    def get_source(self, config, date, is_live_mode):
        if is_live_mode:
            return SubscriptionDataSource("https://www.bitstamp.net/api/ticker/", SubscriptionTransportMedium.REST)

        #return "http://my-ftp-server.com/futures-data-" + date.to_string("Ymd") + ".zip"
        # OR simply return a fixed small data file. Large files will slow down your backtest
        subscription = SubscriptionDataSource("https://www.quantconnect.com/api/v2/proxy/nasdaq/api/v3/datatables/QDL/BITFINEX.csv?code=BTCUSD&api_key=WyAazVXnq7ATy_fefTqm")
        subscription.Sort = True
        return subscription


    def reader(self, config, line, date, is_live_mode):
        coin = Bitcoin()
        coin.symbol = config.symbol

        if is_live_mode:
            # Example Line Format:
            # {"high": "441.00", "last": "421.86", "timestamp": "1411606877", "bid": "421.96", "vwap": "428.58", "volume": "14120.40683975", "low": "418.83", "ask": "421.99"}
            try:
                live_btc = json.loads(line)

                # If value is zero, return None
                value = live_btc["last"]
                if value == 0: return None

                coin.time = datetime.now()
                coin.value = value
                coin["Open"] = float(live_btc["open"])
                coin["High"] = float(live_btc["high"])
                coin["Low"] = float(live_btc["low"])
                coin["Close"] = float(live_btc["last"])
                coin["Ask"] = float(live_btc["ask"])
                coin["Bid"] = float(live_btc["bid"])
                coin["VolumeBTC"] = float(live_btc["volume"])
                coin["WeightedPrice"] = float(live_btc["vwap"])
                return coin
            except ValueError:
                # Do nothing, possible error in json decoding
                return None

        # Example Line Format:
        #code    date        high     low      mid      last     bid      ask      volume
        #BTCUSD  2024-10-08  63248.0  61940.0  62246.5  62245.0  62246.0  62247.0       5.929230648356
        if not (line.strip() and line[7].isdigit()): return None

        try:
            data = line.split(',')
            coin.time = datetime.strptime(data[1], "%Y-%m-%d")
            coin.end_time = coin.time + timedelta(days=1)
            coin.value = float(data[5])
            coin["High"] = float(data[2])
            coin["Low"] = float(data[3])
            coin["Mid"] = float(data[4])
            coin["Close"] = float(data[5])
            coin["Bid"] = float(data[6])
            coin["Ask"] = float(data[7])
            coin["VolumeBTC"] = float(data[8])
            return coin

        except ValueError:
            # Do nothing, possible error in json decoding
            return None
