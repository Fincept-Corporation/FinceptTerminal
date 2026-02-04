# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-90E557F4
# Category: Universe Selection
# Description: This algorithm shows how you can handle universe selection in anyway you like, at any time you like. This algorithm h...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

AddReference("System.Collections")
from System.Collections.Generic import List

### <summary>
### This algorithm shows how you can handle universe selection in anyway you like,
### at any time you like. This algorithm has a list of 10 stocks that it rotates
### through every hour.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="universes" />
### <meta name="tag" content="custom universes" />
class UserDefinedUniverseAlgorithm(QCAlgorithm):

	def initialize(self):
		self.set_cash(100000)
		self.set_start_date(2015,1,1)
		self.set_end_date(2015,12,1)
		self.symbols = [ "SPY", "GOOG", "IBM", "AAPL", "MSFT", "CSCO", "ADBE", "WMT"]

		self.universe_settings.resolution = Resolution.HOUR
		self.add_universe('my_universe_name', Resolution.HOUR, self.selection)

	def selection(self, time):
		index = time.hour%len(self.symbols)
		return [self.symbols[index]]

	def on_data(self, slice):
		pass

	def on_securities_changed(self, changes):
		for removed in changes.removed_securities:
			if removed.invested:
				self.liquidate(removed.symbol)

		for added in changes.added_securities:
			self.set_holdings(added.symbol, 1/float(len(changes.added_securities)))
