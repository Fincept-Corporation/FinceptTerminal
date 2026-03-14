# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-202B8187
# Category: Options
# Description: We add an option contract using 'QCAlgorithm.add_option_contract' and place a trade, the underlying gets deselected f...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### We add an option contract using 'QCAlgorithm.add_option_contract' and place a trade, the underlying
### gets deselected from the universe selection but should still be present since we manually added the option contract.
### Later we call 'QCAlgorithm.remove_option_contract' and expect both option and underlying to be removed.
### </summary>
class AddOptionContractFromUniverseRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2014, 6, 5)
        self.set_end_date(2014, 6, 9)

        self._expiration = datetime(2014, 6, 21)
        self._security_changes = None
        self._option = None
        self._traded = False

        self._twx = Symbol.create("TWX", SecurityType.EQUITY, Market.USA)
        self._aapl = Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)
        self.universe_settings.resolution = Resolution.MINUTE
        self.universe_settings.data_normalization_mode = DataNormalizationMode.RAW

        self.add_universe(self.selector, self.selector)

    def selector(self, fundamental):
        if self.time <= datetime(2014, 6, 5):
            return  [ self._twx ]
        return [ self._aapl ]

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if self._option != None and self.securities[self._option].price != 0 and not self._traded:
            self._traded = True
            self.buy(self._option, 1)

        if self.time == datetime(2014, 6, 6, 14, 0, 0):
            # liquidate & remove the option
            self.remove_option_contract(self._option)

    def on_securities_changed(self, changes):
        # keep track of all removed and added securities
        if self._security_changes == None:
            self._security_changes = changes
        else:
            self._security_changes += changes

        if any(security.symbol.security_type == SecurityType.OPTION for security in changes.added_securities):
            return

        for addedSecurity in changes.added_securities:
            options = self.option_chain(addedSecurity.symbol)
            options = sorted(options, key=lambda x: x.id.symbol)

            option = next((option
                           for option in options
                           if option.id.date == self._expiration and
                           option.id.option_right == OptionRight.CALL and
                           option.id.option_style == OptionStyle.AMERICAN), None)

            self.add_option_contract(option)

            # just keep the first we got
            if self._option == None:
                self._option = option
