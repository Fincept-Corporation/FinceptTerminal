# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-FC1DA597
# Category: Universe Selection
# Description: Custom data universe selection regression algorithm asserting it's behavior. See GH issue #6396
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Custom data universe selection regression algorithm asserting it's behavior. See GH issue #6396
### </summary>
class CustomDataUniverseRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''
        self.set_start_date(2014, 3, 24)
        self.set_end_date(2014, 3, 31)

        self.current_underlying_symbols = set()
        self.universe_settings.resolution = Resolution.DAILY;
        self.add_universe(CoarseFundamental, "custom-data-universe", self.selection)

        self._selection_time = [datetime(2014, 3, 24), datetime(2014, 3, 25), datetime(2014, 3, 26),
                              datetime(2014, 3, 27), datetime(2014, 3, 28), datetime(2014, 3, 29)]

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
                for symbol in sorted(self.current_underlying_symbols, key=lambda x: x.id.symbol):
                    if not self.securities[symbol].has_data:
                        continue;
                    self.set_holdings(symbol, 1 / len(self.current_underlying_symbols))

                    if len([x for x in custom_data.keys() if x.underlying == symbol]) == 0:
                        raise ValueError(f"Custom data was not found for symbol {symbol}")

    def on_end_of_algorithm(self):
        if len(self._selection_time) != 0:
            raise ValueError(f"Unexpected selection times, missing {len(self._selection_time)}")

    def OnSecuritiesChanged(self, changes):
        for security in changes.AddedSecurities:
            if security.symbol.security_type == SecurityType.Base:
                continue
            self.current_underlying_symbols.add(security.Symbol)

        for security in changes.RemovedSecurities:
            if security.symbol.security_type == SecurityType.Base:
                continue
            self.current_underlying_symbols.remove(security.Symbol)

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
