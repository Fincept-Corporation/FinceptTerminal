# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1DD3D3FC
# Category: Futures
# Description: Example algorithm for trading continuous future
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example algorithm for trading continuous future
### </summary>
class BasicTemplateFutureRolloverAlgorithm(QCAlgorithm):

    ### <summary>
    ### Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    ### </summary>
    def initialize(self):
        self.set_start_date(2013, 10, 8)
        self.set_end_date(2013, 12, 10)
        self.set_cash(1000000)

        self._symbol_data_by_symbol = {}
        
        futures = [
            Futures.Indices.SP_500_E_MINI
        ]

        for future in futures:
            # Requesting data
            continuous_contract = self.add_future(future,
                resolution = Resolution.DAILY,
                extended_market_hours = True,
                data_normalization_mode = DataNormalizationMode.BACKWARDS_RATIO,
                data_mapping_mode = DataMappingMode.OPEN_INTEREST,
                contract_depth_offset = 0
            )
            
            symbol_data = SymbolData(self, continuous_contract)
            self._symbol_data_by_symbol[continuous_contract.symbol] = symbol_data

    ### <summary>
    ### on_data event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    ### </summary>
    ### <param name="slice">Slice object keyed by symbol containing the stock data</param>
    def on_data(self, slice):
        for symbol, symbol_data in self._symbol_data_by_symbol.items():
            # Call SymbolData.update() method to handle new data slice received
            symbol_data.update(slice)
            
            # Check if information in SymbolData class and new slice data are ready for trading
            if not symbol_data.is_ready or not slice.bars.contains_key(symbol):
                return
            
            ema_current_value = symbol_data.EMA.current.value
            if ema_current_value < symbol_data.price and not symbol_data.is_long:
                self.market_order(symbol_data.mapped, 1)
            elif ema_current_value > symbol_data.price and not symbol_data.is_short:
                self.market_order(symbol_data.mapped, -1)
    
### <summary>
### Abstracted class object to hold information (state, indicators, methods, etc.) from a Symbol/Security in a multi-security algorithm
### </summary>
class SymbolData:
    
    ### <summary>
    ### Constructor to instantiate the information needed to be hold
    ### </summary>
    def __init__(self, algorithm, future):
        self._algorithm = algorithm
        self._future = future
        self.EMA = algorithm.ema(future.symbol, 20, Resolution.DAILY)
        self.price = 0
        self.is_long = False
        self.is_short = False

        self.reset()
    
    @property
    def symbol(self):
        return self._future.symbol
    
    @property
    def mapped(self):
        return self._future.mapped
    
    @property
    def is_ready(self):
        return self.mapped is not None and self.EMA.is_ready
    
    ### <summary>
    ### Handler of new slice of data received
    ### </summary>
    def update(self, slice):
        if slice.symbol_changed_events.contains_key(self.symbol):
            changed_event = slice.symbol_changed_events[self.symbol]
            old_symbol = changed_event.old_symbol
            new_symbol = changed_event.new_symbol
            tag = f"Rollover - Symbol changed at {self._algorithm.time}: {old_symbol} -> {new_symbol}"
            quantity = self._algorithm.portfolio[old_symbol].quantity

            # Rolling over: to liquidate any position of the old mapped contract and switch to the newly mapped contract
            self._algorithm.liquidate(old_symbol, tag = tag)
            self._algorithm.market_order(new_symbol, quantity, tag = tag)

            self.reset()
        
        self.price = slice.bars[self.symbol].price if slice.bars.contains_key(self.symbol) else self.price
        self.is_long = self._algorithm.portfolio[self.mapped].is_long
        self.is_short = self._algorithm.portfolio[self.mapped].is_short
        
    ### <summary>
    ### reset RollingWindow/indicator to adapt to newly mapped contract, then warm up the RollingWindow/indicator
    ### </summary>
    def reset(self):
        self.EMA.reset()
        self._algorithm.warm_up_indicator(self.symbol, self.EMA, Resolution.DAILY)
            
    ### <summary>
    ### disposal method to remove consolidator/update method handler, and reset RollingWindow/indicator to free up memory and speed
    ### </summary>
    def dispose(self):
        self.EMA.reset()
