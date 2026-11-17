### QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
### Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
###
### Licensed under the Apache License, Version 2.0 (the "License");
### you may not use this file except in compliance with the License.
### You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
###
### Unless required by applicable law or agreed to in writing, software
### distributed under the License is distributed on an "AS IS" BASIS,
### WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
### See the License for the specific language governing permissions and
### limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

from AlgorithmImports import *

### <summary>
### This regression test tests for the loading of futures options contracts with a contract month of 2020-03 can live
### and be loaded from the same ZIP file that the 2020-04 contract month Future Option contract lives in.
### </summary>
class FutureOptionMultipleContractsInDifferentContractMonthsWithSameUnderlyingFutureRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.expected_symbols = {
            self._create_option(datetime(2020, 3, 26), OptionRight.CALL, 1650.0): False,
            self._create_option(datetime(2020, 3, 26), OptionRight.PUT, 1540.0): False,
            self._create_option(datetime(2020, 2, 25), OptionRight.CALL, 1600.0): False,
            self._create_option(datetime(2020, 2, 25), OptionRight.PUT, 1545.0): False
        }
        
        # Required for FOPs to use extended hours, until GH #6491 is addressed
        self.universe_settings.extended_market_hours = True

        self.set_start_date(2020, 1, 4)
        self.set_end_date(2020, 1, 6)

        gold_futures = self.add_future("GC", Resolution.MINUTE, Market.COMEX, extended_market_hours=True)
        gold_futures.SetFilter(0, 365)

        self.add_future_option(gold_futures.Symbol)

    def on_data(self, data: Slice):
        for symbol in data.quote_bars.keys():
            # Check that we are in regular hours, we can place a market order (on extended hours, limit orders should be used)
            if symbol in self.expected_symbols and self.is_in_regular_hours(symbol):
                invested = self.expected_symbols[symbol]
                if not invested:
                    self.market_order(symbol, 1)

                self.expected_symbols[symbol] = True

    def on_end_of_algorithm(self):
        not_encountered = [str(k) for k,v in self.expected_symbols.items() if not v]
        if any(not_encountered):
            raise AggregateException(f"Expected all Symbols encountered and invested in, but the following were not found: {', '.join(not_encountered)}")

        if not self.portfolio.invested:
            raise AggregateException("Expected holdings at the end of algorithm, but none were found.")

    def is_in_regular_hours(self, symbol):
        return self.securities[symbol].exchange.exchange_open

    def _create_option(self, expiry: datetime, option_right: OptionRight, strike_price: float) -> Symbol:
        return Symbol.create_option(
            Symbol.create_future("GC", Market.COMEX, datetime(2020, 4, 28)),
            Market.COMEX,
            OptionStyle.AMERICAN,
            option_right,
            strike_price,
            expiry
        )
