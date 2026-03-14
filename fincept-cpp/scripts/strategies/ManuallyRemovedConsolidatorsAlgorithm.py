# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C0569D9C
# Category: Data Consolidation
# Description: Algorithm asserting that the consolidators are removed from the SubscriptionManager. This makes sure that we don't lo...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Algorithm asserting that the consolidators are removed from the SubscriptionManager.
### This makes sure that we don't lose references to python consolidators when
### they are wrapped in a DataConsolidatorPythonWrapper.
### </summary>
class ManuallyRemovedConsolidatorsAlgorithm(QCAlgorithm):

    def initialize(self) -> None:
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

        self.consolidators = []

        self.spy = self.add_equity("SPY").symbol

        # Case 1: consolidator registered through a RegisterIndicator() call,
        # which will wrap the consolidator instance in a DataConsolidatorPythonWrapper
        consolidator = self.resolve_consolidator(self.spy, Resolution.MINUTE)
        self.consolidators.append(consolidator)

        indicator_name = self.create_indicator_name(self.spy, "close", Resolution.MINUTE)
        identity = Identity(indicator_name)
        self.indicator = self.register_indicator(self.spy, identity, consolidator)

        # Case 2: consolidator registered directly through the SubscriptionManager
        consolidator = self.resolve_consolidator(self.spy, Resolution.MINUTE)
        self.consolidators.append(consolidator)
        self.subscription_manager.add_consolidator(self.spy, consolidator)

        # Case 3: custom python consolidator not derived from IDataConsolidator
        consolidator = CustomQuoteBarConsolidator(timedelta(hours=1))
        self.consolidators.append(consolidator)
        self.subscription_manager.add_consolidator(self.spy, consolidator)

    def on_end_of_algorithm(self) -> None:
        # Remove the first consolidator

        for i in range(3):
            consolidator = self.consolidators[i]
            self.remove_consolidator(consolidator, expected_consolidator_count=2 - i)

    def remove_consolidator(self, consolidator: IDataConsolidator, expected_consolidator_count: int) -> None:
        self.subscription_manager.remove_consolidator(self.spy, consolidator)

        consolidator_count = sum(s.consolidators.count for s in self.subscription_manager.subscriptions)
        if consolidator_count != expected_consolidator_count:
            raise Exception(f"Unexpected number of consolidators after removal. "
                            f"Expected: {expected_consolidator_count}. Actual: {consolidator_count}")

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
