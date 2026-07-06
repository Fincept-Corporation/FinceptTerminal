# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5FE7DA42
# Category: Data Consolidation
# Description: Custom Consolidator Regression Algorithm shows some examples of how to build custom
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class CustomConsolidatorRegressionAlgorithm(QCAlgorithm):
    '''Custom Consolidator Regression Algorithm shows some examples of how to build custom 
    consolidators in Python.'''

    def initialize(self):
        self.set_start_date(2013,10,4)  
        self.set_end_date(2013,10,11)    
        self.set_cash(100000)  
        self.add_equity("SPY", Resolution.MINUTE)

        #Create 5 day QuoteBarConsolidator; set consolidated function; add to subscription manager
        five_day_consolidator = QuoteBarConsolidator(timedelta(days=5))
        five_day_consolidator.data_consolidated += self.on_quote_bar_data_consolidated
        self.subscription_manager.add_consolidator("SPY", five_day_consolidator)

        #Create a 3:10PM custom quote bar consolidator
        timed_consolidator = DailyTimeQuoteBarConsolidator(time(hour=15, minute=10))
        timed_consolidator.data_consolidated += self.on_quote_bar_data_consolidated
        self.subscription_manager.add_consolidator("SPY", timed_consolidator)

        #Create our entirely custom 2 day quote bar consolidator
        self.custom_consolidator = CustomQuoteBarConsolidator(timedelta(days=2))
        self.custom_consolidator.data_consolidated += (self.on_quote_bar_data_consolidated)
        self.subscription_manager.add_consolidator("SPY", self.custom_consolidator)

        #Create an indicator and register a consolidator to it
        self.moving_average = SimpleMovingAverage(5)
        self.custom_consolidator2 = CustomQuoteBarConsolidator(timedelta(hours=1))
        self.register_indicator("SPY", self.moving_average, self.custom_consolidator2)


    def on_quote_bar_data_consolidated(self, sender, bar):
        '''Function assigned to be triggered by consolidators.
        Designed to post debug messages to show how the examples work, including
        which consolidator is posting, as well as its values.

        If using an inherited class and not overwriting OnDataConsolidated
        we expect to see the super C# class as the sender type.

        Using sender.period only works when all consolidators have a Period value.
        '''
        
        consolidator_info = str(type(sender)) + str(sender.period)
       
        self.debug("Bar Type: " + consolidator_info)
        self.debug("Bar Range: " + bar.time.ctime() + " - " + bar.end_time.ctime())
        self.debug("Bar value: " + str(bar.close))
    
    def on_data(self, slice):
        test = slice.get_values()

        if self.custom_consolidator.consolidated and slice.contains_key("SPY"):
            data = slice['SPY']
            
            if self.moving_average.is_ready:
                if data.value > self.moving_average.current.price:
                    self.set_holdings("SPY", .5)
                else :
                    self.set_holdings("SPY", 0)
            


class DailyTimeQuoteBarConsolidator(QuoteBarConsolidator):
    '''A custom QuoteBar consolidator that inherits from C# class QuoteBarConsolidator. 

    This class shows an example of building on top of an existing consolidator class, it is important
    to note that this class can leverage the functions of QuoteBarConsolidator but its private fields
    (_period, _workingbar, etc.) are separate from this Python. For that reason if we want Scan() to work
    we must overwrite the function with our desired Scan function and trigger OnDataConsolidated().
    
    For this particular example we implemented the scan method to trigger a consolidated bar
    at close_time everyday'''

    def __init__(self, close_time):
        self.close_time = close_time
        self.working_bar = None
    
    def update(self, data):
        '''Updates this consolidator with the specified data'''

        #If we don't have bar yet, create one
        if self.working_bar is None:
            self.working_bar = QuoteBar(data.time,data.symbol,data.bid,data.last_bid_size,
                data.ask,data.last_ask_size)

        #Update bar using QuoteBarConsolidator's AggregateBar()
        self.aggregate_bar(self.working_bar, data)
        

    def scan(self, time):
        '''Scans this consolidator to see if it should emit a bar due yet'''

        if self.working_bar is None:
            return

        #If its our desired bar end time take the steps to 
        if time.hour == self.close_time.hour and time.minute == self.close_time.minute:

            #Set end time
            self.working_bar.end_time = time

            #Emit event using QuoteBarConsolidator's OnDataConsolidated()
            self.on_data_consolidated(self.working_bar)

            #Reset the working bar to None
            self.working_bar = None

class CustomQuoteBarConsolidator(PythonConsolidator):
    '''A custom quote bar consolidator that inherits from PythonConsolidator and implements 
    the IDataConsolidator interface, it must implement all of IDataConsolidator. Reference 
    PythonConsolidator.cs and DataConsolidatorPythonWrapper.PY for more information.

    This class shows how to implement a consolidator from scratch in Python, this gives us more
    freedom to determine the behavior of the consolidator but can't leverage any of the built in
    functions of an inherited class.
    
    For this example we implemented a Quotebar from scratch'''

    def __init__(self, period):

        #IDataConsolidator required vars for all consolidators
        self.consolidated = None        #Most recently consolidated piece of data.
        self.working_data = None         #Data being currently consolidated
        self.input_type = QuoteBar       #The type consumed by this consolidator
        self.output_type = QuoteBar      #The type produced by this consolidator

        #Consolidator Variables
        self.period = period
    
    def update(self, data):
        '''Updates this consolidator with the specified data'''
        
        #If we don't have bar yet, create one
        if self.working_data is None:
            self.working_data = QuoteBar(data.time,data.symbol,data.bid,data.last_bid_size,
                data.ask,data.last_ask_size,self.period)

        #Update bar using QuoteBar's update()
        self.working_data.update(data.value, data.bid.close, data.ask.close, 0, 
            data.last_bid_size, data.last_ask_size)

    def scan(self, time):
        '''Scans this consolidator to see if it should emit a bar due to time passing'''

        if self.period is not None and self.working_data is not None:
            if time - self.working_data.time >= self.period:

                #Trigger the event handler with a copy of self and the data
                self.on_data_consolidated(self, self.working_data)

                #Set the most recent consolidated piece of data and then clear the working_data
                self.consolidated = self.working_data
                self.working_data = None
