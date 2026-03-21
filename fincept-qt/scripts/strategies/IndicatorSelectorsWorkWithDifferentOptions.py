# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9EF0798B
# Category: Options
# Description: This regression algorithm tests that we can use selectors in the indicators that need quote data
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### This regression algorithm tests that we can use selectors in the indicators
### that need quote data
### </summary>
class IndicatorSelectorWorksWithDifferentOptions(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013, 6, 7)
        self.set_end_date(2013, 11, 8)
        
        self.equity = self.add_equity("SPY", Resolution.MINUTE).symbol
        self.option = self.add_option("NWSA", Resolution.MINUTE).symbol
        self.aapl = self.add_equity("AAPL", Resolution.DAILY).symbol
        self.aapl_points = []
        self.aapl_last_date = datetime(1,1,1)
        self.eurusd = self.add_forex("EURUSD", Resolution.DAILY).symbol
        self.eurusd_points = []
        self.eurusd_last_date = datetime(1,1,1)
        future = self.add_future("GC", Resolution.DAILY, Market.COMEX)
        self.future = future.symbol
        self.future_contract = None
        self.future_points = []
        future.set_filter(0, 120)
        self.option = Symbol.create_option("NWSA", Market.USA, OptionStyle.AMERICAN, OptionRight.PUT, 33, datetime(2013, 7, 20))
        self.add_option_contract(self.option, Resolution.MINUTE)
        
        self.option_indicator = self.identity(self.option, Resolution.MINUTE, Field.VOLUME, "Volume.")

        self.bid_close_indicator = self.identity(self.equity, Resolution.MINUTE, Field.BID_CLOSE, "Bid.Close.")
        self.bid_open_indicator = self.identity(self.equity, Resolution.MINUTE, Field.BID_OPEN, "Bid.Open.")
        self.bid_low_indicator = self.identity(self.equity, Resolution.MINUTE, Field.BID_LOW, "Bid.Low.")
        self.bid_high_indicator = self.identity(self.equity, Resolution.MINUTE, Field.BID_HIGH, "Bid.High.")
        
        self.ask_close_indicator = self.identity(self.equity, Resolution.MINUTE, Field.ASK_CLOSE, "Ask.Close.")
        self.ask_open_indicator = self.identity(self.equity, Resolution.MINUTE, Field.ASK_OPEN, "Ask.Open.")
        self.ask_low_indicator = self.identity(self.equity, Resolution.MINUTE, Field.ASK_LOW, "Ask.Low.")
        self.ask_high_indicator = self.identity(self.equity, Resolution.MINUTE, Field.ASK_HIGH, "Ask.High.")

        self.quotebars_found = False
        self.tradebars_found = False

        self.tradebar_history_indicator = self.identity(self.aapl, Resolution.DAILY)
        self.quotebar_history_indicator = self.identity(self.eurusd, Resolution.DAILY)

    def on_data(self, slice):
        if self.aapl_last_date != self.time.date:
            bars = slice.get(TradeBar)
            if self.aapl in bars.keys():
                datapoint = bars[self.aapl]
                if datapoint and datapoint.volume != 0:
                    self.aapl_last_date = self.time.date
                    self.aapl_points.append(datapoint.volume)

        if self.eurusd_last_date != self.time.date and (self.eurusd in slice.quote_bars.keys()):
            self.eurusd_last_date = self.time.date
            self.eurusd_points.append(slice.quote_bars[self.eurusd].bid.close)

        if self.equity in slice.quote_bars.keys():
            self.quotebars_found = True
            if slice.quote_bars["SPY"].bid.close != self.bid_close_indicator.current.value:
                close_value = slice.quote_bars["SPY"].bid.close
                raise Exception(f"{self.bid_close_indicator.__name__} should have been {close_value}, but was {self.bid_close_indicator.current.value}")

            if slice.quote_bars["SPY"].bid.open != self.bid_open_indicator.current.value:
                open_value = slice.quote_bars["SPY"].bid.open
                raise Exception(f"{self.bid_open_indicator.__name__} should have been {open_value}, but was {self.bid_open_indicator.current.value}")

            if slice.quote_bars["SPY"].bid.low != self.bid_low_indicator.current.value:
                low_value = slice.quote_bars["SPY"].bid.low
                raise Exception(f"{self.bid_low_indicator.__name__} should have been {low_value}, but was {self.bid_low_indicator.current.value}")

            if slice.quote_bars["SPY"].bid.high != self.bid_high_indicator.current.value:
                high_value = slice.quote_bars["SPY"].bid.high
                raise Exception(f"{self.bid_high_indicator.__name__} should have been {high_value}, but was {self.bid_high_indicator.current.value}")

            if slice.quote_bars["SPY"].ask.close != self.ask_close_indicator.current.value:
                close_value = slice.quote_bars["SPY"].ask.close
                raise Exception(f"{self.ask_close_indicator.__name__} should have been {close_value}, but was {self.ask_close_indicator.current.value}")

            if slice.quote_bars["SPY"].ask.open != self.ask_open_indicator.current.value:
                open_value = slice.quote_bars["SPY"].bid.open
                raise Exception(f"{self.ask_open_indicator.__name__} should have been {open_value}, but was {self.ask_open_indicator.current.value}")

            if slice.quote_bars["SPY"].ask.low != self.ask_low_indicator.current.value:
                low_value = slice.quote_bars["SPY"].bid.low
                raise Exception(f"{self.ask_low_indicator.__name__} should have been {low_value}, but was {self.ask_low_indicator.current.value}")

            if slice.quote_bars["SPY"].ask.high != self.ask_high_indicator.current.value:
                high_value = slice.quote_bars["SPY"].bid.high
                raise Exception(f"{self.ask_high_indicator.__name__} should have been {high_value}, but was {self.ask_high_indicator.current.value}")
        
        if (self.option.canonical in slice.option_chains.keys()) and (self.option in slice.option_chains[self.option.canonical].trade_bars.keys()):
            self.tradebars_found = True
            if self.option_indicator.current.value != slice.option_chains[self.option.canonical].trade_bars[self.option].volume:
                volume = slice.option_chains[self.option.canonical].trade_bars[self.option].volume
                raise Exception(f"{self.option_indicator.__name__} should have been {volume}, but was {self.option_indicator.current.value}")

        if (self.future in slice.futures_chains.keys()):
            if self.future_contract == None:
                self.future_contract = slice.future_chains[self.future].trade_bars.values()[0].symbol
            if self.future_contract in slice.future_chains[self.future].trade_bars:
                value = slice.future_chains[self.future].trade_bars[self.future_contract]
                if value.volume != 0:
                    self.future_points.append(value.volume)
            
    def on_end_of_algorithm(self):
        if not self.quotebars_found:
            raise Exception("At least one quote bar should have been found, but none was found")
        
        if not self.tradebars_found:
            raise Exception("At least one trade bar should have been found, but none was found")

        future_indicator = Identity("")
        backtest_days = (self.end_date - self.start_date).days
        future_volume_history = self.indicator_history(future_indicator, self.future_contract, backtest_days, Resolution.DAILY, Field.VOLUME).current
        future_volume_history_values = list(map(lambda x: x.value, future_volume_history))
        future_volume_history_values = list(filter(lambda x: x != 0, future_volume_history_values))
        if abs(sum(future_volume_history_values)/len(future_volume_history_values) - sum(self.future_points)/len(self.future_points)) > 0.001:
            raise Exception(f"No history indicator future data point was found using Field.Volume selector! {self.future_points}")

        volume_history = self.indicator_history(self.tradebar_history_indicator, self.aapl, 109, Resolution.DAILY, Field.VOLUME).current
        volume_history_values = list(map(lambda x: x.value, volume_history))

        if abs(sum(volume_history_values)/len(volume_history_values) - sum(self.aapl_points)/len(self.aapl_points)) > 0.001:
            raise Exception("No history indicator data point was found using Field.Volume selector!")

        bid_close_history = self.indicator_history(self.quotebar_history_indicator, self.eurusd, 132, Resolution.DAILY, Field.BID_CLOSE).current
        bid_close_history_values = list(map(lambda x: x.value, bid_close_history))
        if abs(sum(bid_close_history_values)/len(bid_close_history_values) - sum(self.eurusd_points)/len(self.eurusd_points)) > 0.001:
            raise Exception("No history indicator data point was found using Field.BidClose selector!")
