# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-6C937E43
# Category: Regression Test
# Description: Tests filtering in coarse selection by shortable quantity
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Tests filtering in coarse selection by shortable quantity
### </summary>
class AllShortableSymbolsCoarseSelectionRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self._20140325 = datetime(2014, 3, 25)
        self._20140326 = datetime(2014, 3, 26)
        self._20140327 = datetime(2014, 3, 27)
        self._20140328 = datetime(2014, 3, 28)
        self._20140329 = datetime(2014, 3, 29)
        self.last_trade_date = datetime(1,1,1)

        self._aapl = Symbol.create("AAPL", SecurityType.EQUITY, Market.USA)
        self._bac = Symbol.create("BAC", SecurityType.EQUITY, Market.USA)
        self._gme = Symbol.create("GME", SecurityType.EQUITY, Market.USA)
        self._goog = Symbol.create("GOOG", SecurityType.EQUITY, Market.USA)
        self._qqq = Symbol.create("QQQ", SecurityType.EQUITY, Market.USA)
        self._spy = Symbol.create("SPY", SecurityType.EQUITY, Market.USA)

        self.coarse_selected = { self._20140325: False, self._20140326: False, self._20140327:False, self._20140328:False }
        self.expected_symbols = { self._20140325: [ self._bac, self._qqq, self._spy ],
                                self._20140326: [ self._spy ],
                                self._20140327: [ self._aapl, self._bac, self._gme, self._qqq, self._spy ],
                                self._20140328: [ self._goog ],
                                self._20140329: []}

        self.set_start_date(2014, 3, 25)
        self.set_end_date(2014, 3, 29)
        self.set_cash(10000000)
        self.shortable_provider = RegressionTestShortableProvider()
        self.security = self.add_equity(self._spy)

        self.add_universe(self.coarse_selection)
        self.universe_settings.resolution = Resolution.DAILY

        self.set_brokerage_model(AllShortableSymbolsRegressionAlgorithmBrokerageModel(self.shortable_provider))

    def on_data(self, data):
        if self.time.date() == self.last_trade_date:
            return

        for (symbol, security) in {x.key: x.value for x in sorted(self.active_securities, key = lambda kvp:kvp.key)}.items():
            if security.invested:
                continue
            shortable_quantity = security.shortable_provider.shortable_quantity(symbol, self.time)
            if not shortable_quantity:
                raise Exception(f"Expected {symbol} to be shortable on {self.time.strftime('%Y%m%d')}")

            """
            Buy at least once into all Symbols. Since daily data will always use
            MOO orders, it makes the testing of liquidating buying into Symbols difficult.
            """
            self.market_order(symbol, -shortable_quantity)
            self.last_trade_date = self.time.date()

    def coarse_selection(self, coarse):
        shortable_symbols = self.shortable_provider.all_shortable_symbols(self.time)
        selected_symbols = list(sorted(filter(lambda x: (x in shortable_symbols.keys()) and (shortable_symbols[x] >= 500), map(lambda x: x.symbol, coarse)), key= lambda x: x.value))

        expected_missing = 0
        if self.time.date() == self._20140327.date():
            gme = Symbol.create("GME", SecurityType.EQUITY, Market.USA)
            if gme not in shortable_symbols.keys():
                raise Exception("Expected unmapped GME in shortable symbols list on 2014-03-27")
            if "GME" not in list(map(lambda x: x.symbol.value, coarse)):
                raise Exception("Expected mapped GME in coarse symbols on 2014-03-27")

            expected_missing = 1

        missing = list(filter(lambda x: x not in selected_symbols, self.expected_symbols[self.time]))
        if len(missing) != expected_missing:
            raise Exception(f"Expected Symbols selected on {self.time.strftime('%Y%m%d')} to match expected Symbols, but the following Symbols were missing: {', '.join(list(map(lambda x:x.value, missing)))}")

        self.coarse_selected[self.time] = True
        return selected_symbols

    def on_end_of_algorithm(self):
        if not all(x for x in self.coarse_selected.values()):
            raise Exception(f"Expected coarse selection on all dates, but didn't run on: {', '.join(list(map(lambda x: x.key.strftime('%Y%m%d'), filter(lambda x:not x.value, self.coarse_selected))))}")

class AllShortableSymbolsRegressionAlgorithmBrokerageModel(DefaultBrokerageModel):
    def __init__(self, shortable_provider):
        self.shortable_provider = shortable_provider
        super().__init__()

    def get_shortable_provider(self, security):
        return self.shortable_provider

class RegressionTestShortableProvider(LocalDiskShortableProvider):
    def __init__(self):
        super().__init__("testbrokerage")

    """
    Gets a list of all shortable Symbols, including the quantity shortable as a Dictionary.
    """
    def all_shortable_symbols(self, localtime):
        shortable_data_directory = os.path.join(Globals.DataFolder, "equity", Market.USA, "shortable", self.brokerage)
        all_symbols = {}

        """
        Check backwards up to one week to see if we can source a previous file.
        If not, then we return a list of all Symbols with quantity set to zero.
        """
        i = 0
        while i <= 7:
            shortable_list_file = os.path.join(shortable_data_directory, "dates", f"{(localtime - timedelta(days=i)).strftime('%Y%m%d')}.csv")

            for line in Extensions.read_lines(self.data_provider, shortable_list_file):
                csv = line.split(',')
                ticker = csv[0]

                symbol = Symbol(SecurityIdentifier.generate_equity(ticker, Market.USA, mapping_resolve_date = localtime), ticker)
                quantity = int(csv[1])
                all_symbols[symbol] = quantity

            if len(all_symbols) > 0:
                return all_symbols

            i += 1

        # Return our empty dictionary if we did not find a file to extract
        return all_symbols
