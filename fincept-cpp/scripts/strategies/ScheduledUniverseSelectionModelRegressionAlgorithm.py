# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-3F154B00
# Category: Universe Selection
# Description: Regression algorithm for testing ScheduledUniverseSelectionModel scheduling functions
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Regression algorithm for testing ScheduledUniverseSelectionModel scheduling functions.
### </summary>
class ScheduledUniverseSelectionModelRegressionAlgorithm(QCAlgorithm):
    '''Regression algorithm for testing ScheduledUniverseSelectionModel scheduling functions.'''

    def initialize(self):

        self.universe_settings.resolution = Resolution.HOUR

        # Order margin value has to have a minimum of 0.5% of Portfolio value, allows filtering out small trades and reduce fees.
        # Commented so regression algorithm is more sensitive
        #self.settings.minimum_order_margin_portfolio_percentage = 0.005

        self.set_start_date(2017, 1, 1)
        self.set_end_date(2017, 2, 1)

        # selection will run on mon/tues/thurs at 00:00/06:00/12:00/18:00
        self.set_universe_selection(ScheduledUniverseSelectionModel(
            self.date_rules.every(DayOfWeek.MONDAY, DayOfWeek.TUESDAY, DayOfWeek.THURSDAY),
            self.time_rules.every(timedelta(hours = 12)),
            self.select_symbols
            ))

        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1)))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        # some days of the week have different behavior the first time -- less securities to remove
        self.seen_days = []

    def select_symbols(self, dateTime):

        symbols = []
        weekday = dateTime.weekday()

        if weekday == 0 or weekday == 1:
            symbols.append(Symbol.create('SPY', SecurityType.EQUITY, Market.USA))
        elif weekday == 2:
            # given the date/time rules specified in Initialize, this symbol will never be selected (not invoked on wednesdays)
            symbols.append(Symbol.create('AAPL', SecurityType.EQUITY, Market.USA))
        else:
            symbols.append(Symbol.create('IBM', SecurityType.EQUITY, Market.USA))

        if weekday == 1 or weekday == 3:
            symbols.append(Symbol.create('EURUSD', SecurityType.FOREX, Market.OANDA))
        elif weekday == 4:
            # given the date/time rules specified in Initialize, this symbol will never be selected (every 6 hours never lands on hour==1)
            symbols.append(Symbol.create('EURGBP', SecurityType.FOREX, Market.OANDA))
        else:
            symbols.append(Symbol.create('NZDUSD', SecurityType.FOREX, Market.OANDA))

        return symbols

    def on_securities_changed(self, changes):
        self.log("{}: {}".format(self.time, changes))

        weekday = self.time.weekday()

        if weekday == 0:
            self.expect_additions(changes, 'SPY', 'NZDUSD')
            if weekday not in self.seen_days:
                self.seen_days.append(weekday)
                self.expect_removals(changes, None)
            else:
                self.expect_removals(changes, 'EURUSD', 'IBM')

        if weekday == 1:
            self.expect_additions(changes, 'EURUSD')
            if weekday not in self.seen_days:
                self.seen_days.append(weekday)
                self.expect_removals(changes, 'NZDUSD')
            else:
                self.expect_removals(changes, 'NZDUSD')

        if weekday == 2 or weekday == 4:
            # selection function not invoked on wednesdays (2) or friday (4)
            self.expect_additions(changes, None)
            self.expect_removals(changes, None)

        if weekday == 3:
            self.expect_additions(changes, "IBM")
            self.expect_removals(changes, "SPY")


    def on_order_event(self, orderEvent):
        self.log("{}: {}".format(self.time, orderEvent))

    def expect_additions(self, changes, *tickers):
        if tickers is None and changes.added_securities.count > 0:
            raise Exception("{}: Expected no additions: {}".format(self.time, self.time.weekday()))

        for ticker in tickers:
            if ticker is not None and ticker not in [s.symbol.value for s in changes.added_securities]:
                raise Exception("{}: Expected {} to be added: {}".format(self.time, ticker, self.time.weekday()))

    def expect_removals(self, changes, *tickers):
        if tickers is None and changes.removed_securities.count > 0:
            raise Exception("{}: Expected no removals: {}".format(self.time, self.time.weekday()))

        for ticker in tickers:
            if ticker is not None and ticker not in [s.symbol.value for s in changes.removed_securities]:
                raise Exception("{}: Expected {} to be removed: {}".format(self.time, ticker, self.time.weekday()))
