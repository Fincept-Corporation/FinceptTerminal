# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5DD0A982
# Category: Data Consolidation
# Description: Example algorithm giving an introduction into using IDataConsolidators. This is an advanced QC concept and requires a...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example algorithm giving an introduction into using IDataConsolidators.
### This is an advanced QC concept and requires a certain level of comfort using C# and its event system.
###
### What is an IDataConsolidator?
### IDataConsolidator is a plugin point that can be used to transform your data more easily.
### In this example we show one of the simplest consolidators, the TradeBarConsolidator.
### This type is capable of taking a timespan to indicate how long each bar should be, or an
### integer to indicate how many bars should be aggregated into one.
###
### When a new 'consolidated' piece of data is produced by the IDataConsolidator, an event is fired
### with the argument of the new data.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="consolidating data" />
class DataConsolidationAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(DateTime(2013, 10, 7, 9, 30, 0))  #Set Start Date
        self.set_end_date(self.start_date + timedelta(60))          #Set End Date
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY")
        self.add_forex("EURUSD", Resolution.HOUR)

        # define our 30 minute trade bar consolidator. we can
        # access the 30 minute bar from the DataConsolidated events
        thirty_minute_consolidator = TradeBarConsolidator(timedelta(minutes=30))

        # attach our event handler. the event handler is a function that will
        # be called each time we produce a new consolidated piece of data.
        thirty_minute_consolidator.data_consolidated += self.thirty_minute_bar_handler

        # this call adds our 30 minute consolidator to
        # the manager to receive updates from the engine
        self.subscription_manager.add_consolidator("SPY", thirty_minute_consolidator)

        # here we'll define a slightly more complex consolidator. what we're trying to produce is
        # a 3 day bar. Now we could just use a single TradeBarConsolidator like above and pass in
        # TimeSpan.from_days(3), but in reality that's not what we want. For time spans of longer than
        # a day we'll get incorrect results around weekends and such. What we really want are tradeable
        # days. So we'll create a daily consolidator, and then wrap it with a 3 count consolidator.

        # first define a one day trade bar -- this produces a consolidated piece of data after a day has passed
        one_day_consolidator = TradeBarConsolidator(timedelta(1))

        # next define our 3 count trade bar -- this produces a consolidated piece of data after it sees 3 pieces of data
        three_count_consolidator = TradeBarConsolidator(3)

        # here we combine them to make a new, 3 day trade bar. The SequentialConsolidator allows composition of
        # consolidators. It takes the consolidated output of one consolidator (in this case, the one_day_consolidator)
        # and pipes it through to the three_count_consolidator.  His output will be a 3 day bar.
        three_one_day_bar = SequentialConsolidator(one_day_consolidator, three_count_consolidator)

        # attach our handler
        three_one_day_bar.data_consolidated += self.three_day_bar_consolidated_handler

        # this call adds our 3 day to the manager to receive updates from the engine
        self.subscription_manager.add_consolidator("SPY", three_one_day_bar)

        # Custom monthly consolidator
        custom_monthly_consolidator = TradeBarConsolidator(self.custom_monthly)
        custom_monthly_consolidator.data_consolidated += self.custom_monthly_handler
        self.subscription_manager.add_consolidator("SPY", custom_monthly_consolidator)

        # API convenience method for easily receiving consolidated data
        self.consolidate("SPY", timedelta(minutes=45), self.forty_five_minute_bar_handler)
        self.consolidate("SPY", Resolution.HOUR, self.hour_bar_handler)
        self.consolidate("EURUSD", Resolution.DAILY, self.daily_eur_usd_bar_handler)

        # API convenience method for easily receiving weekly-consolidated data
        self.consolidate("SPY", Calendar.WEEKLY, self.calendar_trade_bar_handler)
        self.consolidate("EURUSD", Calendar.WEEKLY, self.calendar_quote_bar_handler)

        # API convenience method for easily receiving monthly-consolidated data
        self.consolidate("SPY", Calendar.MONTHLY, self.calendar_trade_bar_handler)
        self.consolidate("EURUSD", Calendar.MONTHLY, self.calendar_quote_bar_handler)

        # API convenience method for easily receiving quarterly-consolidated data
        self.consolidate("SPY", Calendar.QUARTERLY, self.calendar_trade_bar_handler)
        self.consolidate("EURUSD", Calendar.QUARTERLY, self.calendar_quote_bar_handler)

        # API convenience method for easily receiving yearly-consolidated data
        self.consolidate("SPY", Calendar.YEARLY, self.calendar_trade_bar_handler)
        self.consolidate("EURUSD", Calendar.YEARLY, self.calendar_quote_bar_handler)

        # some securities may have trade and quote data available, so we can choose it based on TickType:
        #self.consolidate("BTCUSD", Resolution.HOUR, TickType.TRADE, self.hour_bar_handler)   # to get TradeBar
        #self.consolidate("BTCUSD", Resolution.HOUR, TickType.QUOTE, self.hour_bar_handler)   # to get QuoteBar (default)

        self.consolidated_hour = False
        self.consolidated45_minute = False
        self.__last = None

    def on_data(self, data):
        '''We need to declare this method'''
        pass


    def on_end_of_day(self):
        # close up shop each day and reset our 'last' value so we start tomorrow fresh
        self.liquidate("SPY")
        self.__last = None

    def thirty_minute_bar_handler(self, sender, consolidated):
        '''This is our event handler for our 30 minute trade bar defined above in Initialize(). So each time the
        consolidator produces a new 30 minute bar, this function will be called automatically. The 'sender' parameter
         will be the instance of the IDataConsolidator that invoked the event, but you'll almost never need that!'''

        if self.__last is not None and consolidated.close > self.__last.close:
            self.log(f"{consolidated.time} >> SPY >> LONG  >> 100 >> {self.portfolio['SPY'].quantity}")
            self.order("SPY", 100)

        elif self.__last is not None and consolidated.close < self.__last.close:
            self.log(f"{consolidated.time} >> SPY >> SHORT  >> 100 >> {self.portfolio['SPY'].quantity}")
            self.order("SPY", -100)

        self.__last = consolidated


    def three_day_bar_consolidated_handler(self, sender, consolidated):
        ''' This is our event handler for our 3 day trade bar defined above in Initialize(). So each time the
        consolidator produces a new 3 day bar, this function will be called automatically. The 'sender' parameter
        will be the instance of the IDataConsolidator that invoked the event, but you'll almost never need that!'''
        self.log(f"{consolidated.time} >> Plotting!")
        self.plot(consolidated.symbol.value, "3HourBar", consolidated.close)

    def forty_five_minute_bar_handler(self, consolidated):
        ''' This is our event handler for our 45 minute consolidated defined using the Consolidate method'''
        self.consolidated45_minute = True
        self.log(f"{consolidated.end_time} >> FortyFiveMinuteBarHandler >> {consolidated.close}")

    def hour_bar_handler(self, consolidated):
        '''This is our event handler for our one hour consolidated defined using the Consolidate method'''
        self.consolidated_hour = True
        self.log(f"{consolidated.end_time} >> FortyFiveMinuteBarHandler >> {consolidated.close}")

    def daily_eur_usd_bar_handler(self, consolidated):
        '''This is our event handler for our daily consolidated defined using the Consolidate method'''
        self.log(f"{consolidated.end_time} EURUSD Daily consolidated.")

    def calendar_trade_bar_handler(self, trade_bar):
        self.log(f'{self.time} :: {trade_bar.time} {trade_bar.close}')

    def calendar_quote_bar_handler(self, quote_bar):
        self.log(f'{self.time} :: {quote_bar.time} {quote_bar.close}')

    def custom_monthly(self, dt):
        '''Custom Monthly Func'''
        start = dt.replace(day=1).date()
        end = dt.replace(day=28) + timedelta(4)
        end = (end - timedelta(end.day-1)).date()
        return CalendarInfo(start, end - start)

    def custom_monthly_handler(self, sender, consolidated):
        '''This is our event handler Custom Monthly function'''
        self.log(f"{consolidated.time} >> CustomMonthlyHandler >> {consolidated.close}")

    def on_end_of_algorithm(self):
        if not self.consolidated_hour:
            raise Exception("Expected hourly consolidator to be fired.")

        if not self.consolidated45_minute: 
            raise Exception("Expected 45-minute consolidator to be fired.")
