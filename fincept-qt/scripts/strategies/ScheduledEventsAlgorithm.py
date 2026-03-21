# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9781F28A
# Category: Scheduled Events
# Description: Demonstration of the Scheduled Events features available in QuantConnect
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of the Scheduled Events features available in QuantConnect.
### </summary>
### <meta name="tag" content="scheduled events" />
### <meta name="tag" content="date rules" />
### <meta name="tag" content="time rules" />
class ScheduledEventsAlgorithm(QCAlgorithm):

    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.add_equity("SPY")

        # events are scheduled using date and time rules
        # date rules specify on what dates and event will fire
        # time rules specify at what time on thos dates the event will fire

        # schedule an event to fire at a specific date/time
        self.schedule.on(self.date_rules.on(2013, 10, 7), self.time_rules.at(13, 0), self.specific_time)

        # schedule an event to fire every trading day for a security the
        # time rule here tells it to fire 10 minutes after SPY's market open
        self.schedule.on(self.date_rules.every_day("SPY"), self.time_rules.after_market_open("SPY", 10), self.every_day_after_market_open)

        # schedule an event to fire every trading day for a security the
        # time rule here tells it to fire 10 minutes before SPY's market close
        self.schedule.on(self.date_rules.every_day("SPY"), self.time_rules.before_market_close("SPY", 10), self.every_day_after_market_close)

        # schedule an event to fire on a single day of the week
        self.schedule.on(self.date_rules.every(DayOfWeek.WEDNESDAY), self.time_rules.at(12, 0), self.every_wed_at_noon)

        # schedule an event to fire on certain days of the week
        self.schedule.on(self.date_rules.every(DayOfWeek.MONDAY, DayOfWeek.FRIDAY), self.time_rules.at(12, 0), self.every_mon_fri_at_noon)

        # the scheduling methods return the ScheduledEvent object which can be used for other things here I set
        # the event up to check the portfolio value every 10 minutes, and liquidate if we have too many losses
        self.schedule.on(self.date_rules.every_day(), self.time_rules.every(timedelta(minutes=10)), self.liquidate_unrealized_losses)

        # schedule an event to fire at the beginning of the month, the symbol is optional
        # if specified, it will fire the first trading day for that symbol of the month,
        # if not specified it will fire on the first day of the month
        self.schedule.on(self.date_rules.month_start("SPY"), self.time_rules.after_market_open("SPY"), self.rebalancing_code)


    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.'''
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)


    def specific_time(self):
        self.log(f"SpecificTime: Fired at : {self.time}")


    def every_day_after_market_open(self):
        self.log(f"EveryDay.SPY 10 min after open: Fired at: {self.time}")


    def every_day_after_market_close(self):
        self.log(f"EveryDay.SPY 10 min before close: Fired at: {self.time}")


    def every_wed_at_noon(self):
        self.log(f"Wed at 12pm: Fired at: {self.time}")


    def every_mon_fri_at_noon(self):
        self.log(f"Mon/Fri at 12pm: Fired at: {self.time}")


    def liquidate_unrealized_losses(self):
        ''' if we have over 1000 dollars in unrealized losses, liquidate'''
        if self.portfolio.total_unrealized_profit < -1000:
            self.log(f"Liquidated due to unrealized losses at: {self.time}")
            self.liquidate()


    def rebalancing_code(self):
        ''' Good spot for rebalancing code?'''
        pass
