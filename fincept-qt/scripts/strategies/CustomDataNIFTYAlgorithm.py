# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-8C140F85
# Category: Custom Data
# Description: This demonstration imports indian NSE index "NIFTY" as a tradable security in addition to the USDINR currency pair. W...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This demonstration imports indian NSE index "NIFTY" as a tradable security in addition to the USDINR currency pair. We move into the
### NSE market when the economy is performing well.
### </summary>
### <meta name="tag" content="strategy example" />
### <meta name="tag" content="using data" />
### <meta name="tag" content="custom data" />
class CustomDataNIFTYAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2008, 1, 8)
        self.set_end_date(2014, 7, 25)
        self.set_cash(100000)

        # Define the symbol and "type" of our generic data:
        rupee = self.add_data(DollarRupee, "USDINR", Resolution.DAILY).symbol
        nifty = self.add_data(Nifty, "NIFTY", Resolution.DAILY).symbol

        self.settings.automatic_indicator_warm_up = True
        rupee_sma = self.sma(rupee, 20)
        nifty_sma = self.sma(rupee, 20)
        self.log(f"SMA - Is ready? USDINR: {rupee_sma.is_ready} NIFTY: {nifty_sma.is_ready}")

        self.minimum_correlation_history = 50
        self.today = CorrelationPair()
        self.prices = []


    def on_data(self, data):
        if data.contains_key("USDINR"):
            self.today = CorrelationPair(self.time)
            self.today.currency_price = data["USDINR"].close

        if not data.contains_key("NIFTY"): return

        self.today.nifty_price = data["NIFTY"].close

        if self.today.date() == data["NIFTY"].time.date():
            self.prices.append(self.today)
            if len(self.prices) > self.minimum_correlation_history:
                self.prices.pop(0)

        # Strategy
        if self.time.weekday() != 2: return

        cur_qnty = self.portfolio["NIFTY"].quantity
        quantity = int(self.portfolio.margin_remaining * 0.9 / data["NIFTY"].close)
        hi_nifty = max(price.nifty_price for price in self.prices)
        lo_nifty = min(price.nifty_price for price in self.prices)

        if data["NIFTY"].open >= hi_nifty:
            code = self.order("NIFTY",  quantity - cur_qnty)
            self.debug("LONG  {0} Time: {1} Quantity: {2} Portfolio: {3} Nifty: {4} Buying Power: {5}".format(code, self.time, quantity, self.portfolio["NIFTY"].quantity, data["NIFTY"].close, self.portfolio.total_portfolio_value))
        elif data["NIFTY"].open <= lo_nifty:
            code = self.order("NIFTY", -quantity - cur_qnty)
            self.debug("SHORT {0} Time: {1} Quantity: {2} Portfolio: {3} Nifty: {4} Buying Power: {5}".format(code, self.time, quantity, self.portfolio["NIFTY"].quantity, data["NIFTY"].close, self.portfolio.total_portfolio_value))


class Nifty(PythonData):
    '''NIFTY Custom Data Class'''
    def get_source(self, config, date, is_live_mode):
        return SubscriptionDataSource("https://www.dropbox.com/s/rsmg44jr6wexn2h/CNXNIFTY.csv?dl=1", SubscriptionTransportMedium.REMOTE_FILE)


    def reader(self, config, line, date, is_live_mode):
        if not (line.strip() and line[0].isdigit()): return None

        # New Nifty object
        index = Nifty()
        index.symbol = config.symbol

        try:
            # Example File Format:
            # Date,       Open       High        Low       Close     Volume      Turnover
            # 2011-09-13  7792.9    7799.9     7722.65    7748.7    116534670    6107.78
            data = line.split(',')
            index.time = datetime.strptime(data[0], "%Y-%m-%d")
            index.end_time = index.time + timedelta(days=1)
            index.value = data[4]
            index["Open"] = float(data[1])
            index["High"] = float(data[2])
            index["Low"] = float(data[3])
            index["Close"] = float(data[4])


        except ValueError:
                # Do nothing
                return None

        return index


class DollarRupee(PythonData):
    '''Dollar Rupe is a custom data type we create for this algorithm'''
    def get_source(self, config, date, is_live_mode):
        return SubscriptionDataSource("https://www.dropbox.com/s/m6ecmkg9aijwzy2/USDINR.csv?dl=1", SubscriptionTransportMedium.REMOTE_FILE)

    def reader(self, config, line, date, is_live_mode):
        if not (line.strip() and line[0].isdigit()): return None

        # New USDINR object
        currency = DollarRupee()
        currency.symbol = config.symbol

        try:
            data = line.split(',')
            currency.time = datetime.strptime(data[0], "%Y-%m-%d")
            currency.end_time = currency.time + timedelta(days=1)
            currency.value = data[1]
            currency["Close"] = float(data[1])

        except ValueError:
            # Do nothing
            return None

        return currency


class CorrelationPair:
    '''Correlation Pair is a helper class to combine two data points which we'll use to perform the correlation.'''
    def __init__(self, *args):
        self.nifty_price = 0        # Nifty price for this correlation pair
        self.currency_price = 0     # Currency price for this correlation pair
        self._date = datetime.min    # Date of the correlation pair
        if len(args) > 0: self._date = args[0]

    def date(self):
        return self._date.date()
