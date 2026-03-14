# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C5F5B3E0
# Category: General Strategy
# Description: Live Trading Functionality Demonstration algorithm including SMS, Email and Web hook notifications
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from System.Globalization import *

### <summary>
### Live Trading Functionality Demonstration algorithm including SMS, Email and Web hook notifications.
### </summary>
### <meta name="tag" content="live trading" />
### <meta name="tag" content="alerts" />
### <meta name="tag" content="sms alerts" />
### <meta name="tag" content="web hooks" />
### <meta name="tag" content="email alerts" />
### <meta name="tag" content="runtime statistics" />

class LiveTradingFeaturesAlgorithm(QCAlgorithm):

    ### Initialize the Algorithm and Prepare Required Data
    def initialize(self):

        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(25000)

        ##Equity Data for US Markets
        self.add_security(SecurityType.EQUITY, 'IBM', Resolution.SECOND)

        ##FOREX Data for Weekends: 24/6
        self.add_security(SecurityType.FOREX, 'EURUSD', Resolution.MINUTE)

        ##Custom/Bitcoin Live Data: 24/7
        self.add_data(Bitcoin, 'BTC', Resolution.SECOND, TimeZones.UTC)

        ##if the algorithm is connected to the brokerage
        self.is_connected = True


    ### New Bitcoin Data Event
    def on_data(Bitcoin, data):
        if self.live_mode:
            self.set_runtime_statistic('BTC', str(data.close))

        if not self.portfolio.hold_stock:
            self.market_order('BTC', 100)

            ##Send a notification email/SMS/web request on events:
            self.notify.email("myemail@gmail.com", "Test", "Test Body", "test attachment")
            self.notify.sms("+11233456789", str(data.time) + ">> Test message from live BTC server.")
            self.notify.web("http://api.quantconnect.com", str(data.time) + ">> Test data packet posted from live BTC server.")
            self.notify.ftp("ftp.quantconnect.com", "username", "password", "path/to/file.txt",
                            str(data.time) + ">> Test file from live BTC server.")
            self.notify.sftp("ftp.quantconnect.com", "username", "password", "path/to/file.txt",
                             str(data.time) + ">> Test file from live BTC server.")
            self.notify.sftp("ftp.quantconnect.com", "username", "privatekey", "optionalprivatekeypassphrase", "path/to/file.txt",
                             str(data.time) + ">> Test file from live BTC server.")

    ### Raises the data event
    def on_data(self, data):
        if (not self.portfolio['IBM'].hold_stock) and data.contains_key('IBM'):
            quantity = int(np.floor(self.portfolio.margin_remaining / data['IBM'].close))
            self.market_order('IBM',quantity)
            self.debug('Purchased IBM on ' + str(self.time.strftime("%m/%d/%Y")))
            self.notify.email("myemail@gmail.com", "Test", "Test Body", "test attachment")

    # Brokerage message event handler. This method is called for all types of brokerage messages.
    def on_brokerage_message(self, message_event):
        self.debug(f"Brokerage meesage received - {message_event.to_string()}")

    # Brokerage disconnected event handler. This method is called when the brokerage connection is lost.
    def on_brokerage_disconnect(self):
        self.is_connected = False
        self.debug(f"Brokerage disconnected!")

    # Brokerage reconnected event handler. This method is called when the brokerage connection is restored after a disconnection.
    def on_brokerage_reconnect(self):
        self.is_connected = True
        self.debug(f"Brokerage reconnected!")

###Custom Data Type: Bitcoin data from Quandl - http://www.quandl.com/help/api-for-bitcoin-data
class Bitcoin(PythonData):

    def get_source(self, config, date, is_live_mode):
        if is_live_mode:
            return SubscriptionDataSource("https://www.bitstamp.net/api/ticker/", SubscriptionTransportMedium.REST)

        return  SubscriptionDataSource("https://www.quandl.com/api/v3/datasets/BCHARTS/BITSTAMPUSD.csv?order=asc", SubscriptionTransportMedium.REMOTE_FILE)


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
        # Date      Open   High    Low     Close   Volume (BTC)    Volume (Currency)   Weighted Price
        # 2011-09-13 5.8    6.0     5.65    5.97    58.37138238,    346.0973893944      5.929230648356
        if not (line.strip() and line[0].isdigit()): return None

        try:
            data = line.split(',')
            coin.time = datetime.strptime(data[0], "%Y-%m-%d")
            coin.value = float(data[4])
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
