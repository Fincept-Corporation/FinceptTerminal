# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-662DC1F1
# Category: Options
# Description: Demonstration of the Option Chain Provider -- a much faster mechanism for manually specifying the option contracts yo...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Demonstration of the Option Chain Provider -- a much faster mechanism for manually specifying the option contracts you'd like to recieve
### data for and manually subscribing to them.
### </summary>
### <meta name="tag" content="strategy example" />
### <meta name="tag" content="options" />
### <meta name="tag" content="using data" />
### <meta name="tag" content="selecting options" />
### <meta name="tag" content="manual selection" />

class OptionChainProviderAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(100000)
        # add the underlying asset
        self.equity = self.add_equity("GOOG", Resolution.MINUTE)
        self.equity.set_data_normalization_mode(DataNormalizationMode.RAW)
        # initialize the option contract with empty string
        self.contract = str()
        self.contracts_added = set()

    def on_data(self, data):

        if not self.portfolio[self.equity.symbol].invested:
            self.market_order(self.equity.symbol, 100)

        if not (self.securities.contains_key(self.contract) and self.portfolio[self.contract].invested):
            self.contract = self.options_filter(data)

        if self.securities.contains_key(self.contract) and not self.portfolio[self.contract].invested:
            self.market_order(self.contract, -1)

    def options_filter(self, data):
        ''' OptionChainProvider gets a list of option contracts for an underlying symbol at requested date.
            Then you can manually filter the contract list returned by GetOptionContractList.
            The manual filtering will be limited to the information included in the Symbol
            (strike, expiration, type, style) and/or prices from a History call '''

        contracts = self.option_chain_provider.get_option_contract_list(self.equity.symbol, data.time)
        self.underlying_price = self.securities[self.equity.symbol].price
        # filter the out-of-money call options from the contract list which expire in 10 to 30 days from now on
        otm_calls = [i for i in contracts if i.id.option_right == OptionRight.CALL and
                                            i.id.strike_price - self.underlying_price > 0 and
                                            10 < (i.id.date - data.time).days < 30]
        if len(otm_calls) > 0:
            contract = sorted(sorted(otm_calls, key = lambda x: x.id.date),
                                                     key = lambda x: x.id.strike_price - self.underlying_price)[0]
            if contract not in self.contracts_added:
                self.contracts_added.add(contract)
                # use AddOptionContract() to subscribe the data for specified contract
                self.add_option_contract(contract, Resolution.MINUTE)
            return contract
        else:
            return str()
