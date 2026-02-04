# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0D7003A5
# Category: Custom Data
# Description: Demonstration of using an external custom datasource. LEAN Engine is incredibly flexible and allows you to define you...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of using an external custom datasource. LEAN Engine is incredibly flexible and allows you to define your own data source.
### This includes any data source which has a TIME and VALUE. These are the *only* requirements. To demonstrate this we're loading in "Bitcoin" data.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="custom data" />
### <meta name="tag" content="crypto" />
class CustomDataBitcoinAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2011, 9, 13)
        self.set_end_date(datetime.now().date() - timedelta(1))
        self.set_cash(100000)

        # Define the symbol and "type" of our generic data:
        self.add_data(Bitcoin, "BTC")


    def on_data(self, data):
        if not data.contains_key("BTC"): return

        close = data["BTC"].close

        # If we don't have any weather "SHARES" -- invest"
        if not self.portfolio.invested:
            # Weather used as a tradable asset, like stocks, futures etc.
            # It's only OK to use SetHoldings with crypto when using custom data. When trading with built-in crypto data, 
            # use the cashbook. Reference https://github.com/QuantConnect/Lean/blob/master/Algorithm.python/BasicTemplateCryptoAlgorithm.py 
            self.set_holdings("BTC", 1) 
            self.debug("Buying BTC 'Shares': BTC: {0}".format(close))

        self.debug("Time: {0} {1}".format(datetime.now(), close))


class Bitcoin(PythonData):
    '''Custom Data Type: Bitcoin data from Quandl - http://www.quandl.com/help/api-for-bitcoin-data'''

    def get_source(self, config, date, is_live_mode):
        if is_live_mode:
            return SubscriptionDataSource("https://www.bitstamp.net/api/ticker/", SubscriptionTransportMedium.REST)

        #return "http://my-ftp-server.com/futures-data-" + date.to_string("Ymd") + ".zip"
        # OR simply return a fixed small data file. Large files will slow down your backtest
        return SubscriptionDataSource("https://www.quantconnect.com/api/v2/proxy/quandl/api/v3/datasets/BCHARTS/BITSTAMPUSD.csv?order=asc&api_key=WyAazVXnq7ATy_fefTqm", SubscriptionTransportMedium.REMOTE_FILE)


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

                coin.end_time =  datetime.utcnow().astimezone(timezone(str(config.exchange_time_zone))).replace(tzinfo=None)
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
        # Date      Open   High    Low     Close   Volume (BTC)    Volume (Currency)   Weighted Price
        # 2011-09-13 5.8    6.0     5.65    5.97    58.37138238,    346.0973893944      5.929230648356
        if not (line.strip() and line[0].isdigit()): return None

        try:
            data = line.split(',')

            # If value is zero, return None
            value = data[4]
            if value == 0: return None

            coin.time = datetime.strptime(data[0], "%Y-%m-%d")
            coin.end_time = coin.time + timedelta(days=1)
            coin.value = value
            coin["Open"] = float(data[1])
            coin["High"] = float(data[2])
            coin["Low"] = float(data[3])
            coin["Close"] = float(data[4])
            coin["VolumeBTC"] = float(data[5])
            coin["VolumeUSD"] = float(data[6])
            coin["WeightedPrice"] = float(data[7])
            return coin

        except ValueError:
            # Do nothing, possible error in json decoding
            return None
