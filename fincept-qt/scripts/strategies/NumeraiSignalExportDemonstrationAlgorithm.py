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
### This algorithm sends a list of current portfolio targets to Numerai API before each trading day
### See (https://docs.numer.ai/numerai-signals/signals-overview) for more information
### about accepted symbols, signals, etc.
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="securities and portfolio" />
class NumeraiSignalExportDemonstrationAlgorithm(QCAlgorithm):

    securities = []

    def initialize(self):
        ''' Initialize the date and add all equity symbols present in list _symbols '''

        self.set_start_date(2020, 10, 7)   #Set Start Date
        self.set_end_date(2020, 10, 12)    #Set End Date
        self.set_cash(100000)             #Set Strategy Cash

        self.set_security_initializer(BrokerageModelSecurityInitializer(self.brokerage_model, FuncSecuritySeeder(self.get_last_known_prices)))

        # Add the CRSP US Total Market Index constituents, which represents approximately 100% of the investable US Equity market
        self.etf_symbol = self.add_equity("VTI").symbol
        self.add_universe(self.universe.etf(self.etf_symbol))

        # Create a Scheduled Event to submit signals every trading day at 13:00 UTC
        self.schedule.on(self.date_rules.every_day(self.etf_symbol), self.time_rules.at(13, 0, TimeZones.UTC), self.submit_signals)

        # Set Numerai signal export provider
        # Numerai Public ID: This value is provided by Numerai Signals in their main webpage once you've logged in
        # and created a API key. See (https://signals.numer.ai/account)
        numerai_public_id = ""

        # Numerai Secret ID: This value is provided by Numerai Signals in their main webpage once you've logged in
        # and created a API key. See (https://signals.numer.ai/account)
        numerai_secret_id = ""

        # Numerai Model ID: This value is provided by Numerai Signals in their main webpage once you've logged in
        # and created a model. See (https://signals.numer.ai/models)
        numerai_model_id = ""

        numerai_filename = "" # (Optional) Replace this value with your submission filename 
        self.signal_export.add_signal_export_providers(NumeraiSignalExport(numerai_public_id, numerai_secret_id, numerai_model_id, numerai_filename))


    def submit_signals(self):
        # Select the subset of ETF constituents we can trade
        symbols = sorted([security.symbol for security in self.securities if security.has_data])
        if len(symbols) == 0:
            return

        # Get historical data
        # close_prices = self.history(symbols, 22, Resolution.DAILY).close.unstack(0)
        
        # Create portfolio targets
        #  Numerai requires that at least one of the signals have a unique weight
        #  To ensure they are all unique, this demo gives a linear allocation to each symbol (ie. 1/55, 2/55, ..., 10/55)
        denominator = len(symbols) * (len(symbols) + 1) / 2 # sum of 1, 2, ..., len(symbols)
        targets = [PortfolioTarget(symbol, (i+1) / denominator) for i, symbol in enumerate(symbols)]

        # (Optional) Place trades
        self.set_holdings(targets)

        # Send signals to Numerai
        success = self.signal_export.set_target_portfolio(targets)
        if not success:
            self.debug(f"Couldn't send targets at {self.time}")


    def on_securities_changed(self, changes: SecurityChanges) -> None:
        for security in changes.removed_securities:
            if security in self.securities:
                self.securities.remove(security)
                
        self.securities.extend([security for security in changes.added_securities if security.symbol != self.etf_symbol])
