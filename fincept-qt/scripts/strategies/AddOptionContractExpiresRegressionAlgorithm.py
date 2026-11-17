# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

from AlgorithmImports import *

### <summary>
### We add an option contract using 'QCAlgorithm.add_option_contract' and place a trade, the underlying
### gets deselected from the universe selection but should still be present since we manually added the option contract.
### Later we call 'QCAlgorithm.remove_option_contract' and expect both option and underlying to be removed.
### </summary>
class AddOptionContractExpiresRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2014, 6, 5)
        self.set_end_date(2014, 6, 30)

        self._expiration = datetime(2014, 6, 21)
        self._option = None
        self._traded = False

        self._twx = Symbol.create("TWX", SecurityType.EQUITY, Market.USA)

        self.add_universe("my-daily-universe-name", self.selector)

    def selector(self, time):
        return [ "AAPL" ]

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if self._option == None:
            options = self.option_chain(self._twx)
            options = sorted(options, key=lambda x: x.id.symbol)

            option = next((option
                           for option in options
                           if option.id.date == self._expiration and
                           option.id.option_right == OptionRight.CALL and
                           option.id.option_style == OptionStyle.AMERICAN), None)
            if option != None:
                self._option = self.add_option_contract(option).symbol

        if self._option != None and self.securities[self._option].price != 0 and not self._traded:
            self._traded = True
            self.buy(self._option, 1)

        if self.time > self._expiration and self.securities[self._twx].invested:
                # we liquidate the option exercised position
                self.liquidate(self._twx)
