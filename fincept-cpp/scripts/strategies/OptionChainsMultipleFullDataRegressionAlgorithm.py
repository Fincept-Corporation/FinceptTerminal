# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-90DAF80B
# Category: Options
# Description: Regression algorithm illustrating the usage of the <see cref="QCAlgorithm.OptionChains(IEnumerable{Symbol})"/> method...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from datetime import timedelta

### <summary>
### Regression algorithm illustrating the usage of the <see cref="QCAlgorithm.OptionChains(IEnumerable{Symbol})"/> method
### to get multiple option chains, which contains additional data besides the symbols, including prices, implied volatility and greeks.
### It also shows how this data can be used to filter the contracts based on certain criteria.
### </summary>
class OptionChainsMultipleFullDataRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2015, 12, 24)
        self.set_end_date(2015, 12, 24)
        self.set_cash(100000)

        goog = self.add_equity("GOOG").symbol
        spx = self.add_index("SPX").symbol

        chains = self.option_chains([goog, spx], flatten=True)

        self._goog_option_contract = self.get_contract(chains, goog, timedelta(days=10))
        self._spx_option_contract = self.get_contract(chains, spx, timedelta(days=60))

        self.add_option_contract(self._goog_option_contract)
        self.add_index_option_contract(self._spx_option_contract)

    def get_contract(self, chains: OptionChains, underlying: Symbol, expiry_span: timedelta) -> Symbol:
        df = chains.data_frame

        # Index by the requested underlying, by getting all data with canonicals which underlying is the requested underlying symbol:
        canonicals = df.index.get_level_values('canonical')
        condition = [canonical for canonical in canonicals if canonical.underlying == underlying]
        df = df.loc[condition]

        # Get contracts expiring in the next 10 days with an implied volatility greater than 0.5 and a delta less than 0.5
        contracts = df.loc[(df.expiry <= self.time + expiry_span) & (df.impliedvolatility > 0.5) & (df.delta < 0.5)]

        # Select the contract with the latest expiry date
        contracts.sort_values(by='expiry', ascending=False, inplace=True)

        # Get the symbol: the resulting series name is a tuple (canonical symbol, contract symbol)
        return contracts.iloc[0].name[1]

    def on_data(self, data):
        # Do some trading with the selected contract for sample purposes
        if not self.portfolio.invested:
            self.market_order(self._goog_option_contract, 1)
        else:
            self.liquidate()
