# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-BEF6D76E
# Category: Regression Test
# Description: Regression algorithm demonstrating use of map files with custom data
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm demonstrating use of map files with custom data
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="custom data" />
### <meta name="tag" content="regression test" />
### <meta name="tag" content="rename event" />
### <meta name="tag" content="map" />
### <meta name="tag" content="mapping" />
### <meta name="tag" content="map files" />
class CustomDataUsingMapFileRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
        self.set_start_date(2013, 6, 27)
        self.set_end_date(2013, 7, 2)

        self.initial_mapping = False
        self.execution_mapping = False
        self.foxa = Symbol.create("FOXA", SecurityType.EQUITY, Market.USA)
        self._symbol = self.add_data(CustomDataUsingMapping, self.foxa).symbol

        for config in self.subscription_manager.subscription_data_config_service.get_subscription_data_configs(self._symbol):
            if config.resolution != Resolution.MINUTE:
                raise ValueError("Expected resolution to be set to Minute")

    def on_data(self, slice):
        date = self.time.date()
        if slice.symbol_changed_events.contains_key(self._symbol):
            mapping_event = slice.symbol_changed_events[self._symbol]
            self.log("{0} - Ticker changed from: {1} to {2}".format(str(self.time), mapping_event.old_symbol, mapping_event.new_symbol))

            if date == datetime(2013, 6, 27).date():
                # we should Not receive the initial mapping event
                if mapping_event.new_symbol != "NWSA" or mapping_event.old_symbol != "FOXA":
                    raise Exception("Unexpected mapping event mapping_event")
                self.initial_mapping = True

            if date == datetime(2013, 6, 29).date():
                if mapping_event.new_symbol != "FOXA" or mapping_event.old_symbol != "NWSA":
                    raise Exception("Unexpected mapping event mapping_event")
                self.set_holdings(self._symbol, 1)
                self.execution_mapping = True

    def on_end_of_algorithm(self):
        if self.initial_mapping:
            raise Exception("The ticker generated the initial rename event")
        if not self.execution_mapping:
            raise Exception("The ticker did not rename throughout the course of its life even though it should have")

class CustomDataUsingMapping(PythonData):
    '''Test example custom data showing how to enable the use of mapping.
    Implemented as a wrapper of existing NWSA->FOXA equity'''

    def get_source(self, config, date, is_live_mode):
        return TradeBar().get_source(SubscriptionDataConfig(config, CustomDataUsingMapping,
            # create a new symbol as equity so we find the existing data files
            Symbol.create(config.mapped_symbol, SecurityType.EQUITY, config.market)),
            date,
            is_live_mode)

    def reader(self, config, line, date, is_live_mode):
        return TradeBar.parse_equity(config, line, date)

    def requires_mapping(self):
        '''True indicates mapping should be done'''
        return True

    def is_sparse_data(self):
        '''Indicates that the data set is expected to be sparse'''
        return True

    def default_resolution(self):
        '''Gets the default resolution for this data and security type'''
        return Resolution.MINUTE

    def supported_resolutions(self):
        '''Gets the supported resolution for this data and security type'''
        return [ Resolution.MINUTE ]
