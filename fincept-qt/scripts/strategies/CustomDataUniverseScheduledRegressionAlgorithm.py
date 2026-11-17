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
### Custom data universe selection regression algorithm asserting it's behavior. Similar to CustomDataUniverseRegressionAlgorithm but with a custom schedule
### </summary>
class CustomDataUniverseScheduledRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2014, 3, 24)
        self.set_end_date(2014, 3, 31)

        self.current_underlying_symbols = []
        self._selection_time = [datetime(2014, 3, 25), datetime(2014, 3, 27), datetime(2014, 3, 29)]
        self.universe_settings.resolution = Resolution.DAILY;
        self.universe_settings.schedule.on(self.date_rules.on(self._selection_time))
        self.add_universe(CoarseFundamental, "custom-data-universe", self.universe_settings, self.selection)

        # This use case is also valid/same because it will use the algorithm settings by default
        # self.add_universe(CoarseFundamental, "custom-data-universe", self.selection)

    def selection(self, coarse):
        self.debug(f"Universe selection called: {self.time} Count: {len(coarse)}")

        expected_time = self._selection_time.pop(0)
        if expected_time != self.time:
            raise ValueError(f"Unexpected selection time {self.time} expected {expected_time}")

        # sort descending by daily dollar volume
        sorted_by_dollar_volume = sorted(coarse, key=lambda x: x.dollar_volume, reverse=True)

        # return the symbol objects of the top entries from our sorted collection
        underlying_symbols = [ x.symbol for x in sorted_by_dollar_volume[:10] ]
        custom_symbols = []
        for symbol in underlying_symbols:
            custom_symbols.append(Symbol.create_base(MyPyCustomData, symbol))
        return underlying_symbols + custom_symbols

    def on_data(self, data):
        '''OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.

        Arguments:
            data: Slice object keyed by symbol containing the stock data
        '''
        if not self.portfolio.invested:
            custom_data = data.get(MyPyCustomData)
            if len(custom_data) > 0:
                for symbol in self.current_underlying_symbols:
                    self.set_holdings(symbol, 1 / len(self.current_underlying_symbols))

                    if len([x for x in custom_data.keys() if x.underlying == symbol]) == 0:
                        raise ValueError(f"Custom data was not found for symbol {symbol}")
        # equity daily data arrives at 16 pm but custom data is set to arrive at midnight
        self.current_underlying_symbols = [symbol for symbol in data.keys() if symbol.security_type is SecurityType.EQUITY]

    def on_end_of_algorithm(self):
        if len(self._selection_time) != 0:
            raise ValueError(f"Unexpected selection times, missing {len(self._selection_time)}")

class MyPyCustomData(PythonData):

    def get_source(self, config, date, is_live_mode):
        source = f"{Globals.DataFolder}/equity/usa/daily/{LeanData.generate_zip_file_name(config.symbol, date, config.resolution, config.tick_type)}"
        return SubscriptionDataSource(source)

    def reader(self, config, line, date, is_live_mode):
        csv = line.split(',')
        _scaleFactor = 1 / 10000

        custom = MyPyCustomData()
        custom.symbol = config.symbol
        custom.time =  datetime.strptime(csv[0], '%Y%m%d %H:%M')
        custom.open = float(csv[1]) * _scaleFactor
        custom.high = float(csv[2]) * _scaleFactor
        custom.low = float(csv[3]) * _scaleFactor
        custom.close = float(csv[4]) * _scaleFactor
        custom.value = float(csv[4]) * _scaleFactor
        custom.period = Time.ONE_DAY
        custom.end_time = custom.time + custom.period

        return custom
