# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-65D25230
# Category: Alpha Model
# Description: CapmAlphaRankingFrameworkAlgorithm: example of custom scheduled universe selection model Universe Selection inspired ...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### CapmAlphaRankingFrameworkAlgorithm: example of custom scheduled universe selection model
### Universe Selection inspired by https://www.quantconnect.com/tutorials/strategy-library/capm-alpha-ranking-strategy-on-dow-30-companies
### </summary>
class CapmAlphaRankingFrameworkAlgorithm(QCAlgorithm):
    '''CapmAlphaRankingFrameworkAlgorithm: example of custom scheduled universe selection model'''

    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.MINUTE

        self.set_start_date(2016, 1, 1)   #Set Start Date
        self.set_end_date(2017, 1, 1)     #Set End Date
        self.set_cash(100000)            #Set Strategy Cash

        # set algorithm framework models
        self.set_universe_selection(CapmAlphaRankingUniverseSelectionModel())
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(1), 0.025, None))
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(MaximumDrawdownPercentPerSecurity(0.01))

class CapmAlphaRankingUniverseSelectionModel(UniverseSelectionModel):
    '''This universe selection model picks stocks with the highest alpha: interception of the linear regression against a benchmark.'''

    period = 21
    benchmark = "SPY"

    # Symbols of Dow 30 companies.
    symbols = [Symbol.create(x, SecurityType.EQUITY, Market.USA)
               for x in ["AAPL", "AXP", "BA", "CAT", "CSCO", "CVX", "DD", "DIS", "GE", "GS",
                         "HD", "IBM", "INTC", "JPM", "KO", "MCD", "MMM", "MRK", "MSFT",
                         "NKE","PFE", "PG", "TRV", "UNH", "UTX", "V", "VZ", "WMT", "XOM"]]

    def create_universes(self, algorithm):

        # Adds the benchmark to the user defined universe
        benchmark = algorithm.add_equity(self.benchmark, Resolution.DAILY)

        # Defines a schedule universe that fires after market open when the month starts
        return [ ScheduledUniverse(
            benchmark.exchange.time_zone,
            algorithm.date_rules.month_start(self.benchmark),
            algorithm.time_rules.after_market_open(self.benchmark),
            lambda datetime: self.select_pair(algorithm, datetime),
            algorithm.universe_settings,
            algorithm.security_initializer)]

    def select_pair(self, algorithm, date):
        '''Selects the pair (two stocks) with the highest alpha'''
        dictionary = dict()
        benchmark = self._get_returns(algorithm, self.benchmark)
        ones = np.ones(len(benchmark))

        for symbol in self._symbols:
            prices = self._get_returns(algorithm, symbol)
            if prices is None: continue
            A = np.vstack([prices, ones]).T

            # Calculate the Least-Square fitting to the returns of a given symbol and the benchmark
            ols = np.linalg.lstsq(A, benchmark)[0]
            dictionary[symbol] = ols[1]

        # Returns the top 2 highest alphas
        ordered_dictionary = sorted(dictionary.items(), key= lambda x: x[1], reverse=True)
        return [x[0] for x in ordered_dictionary[:2]]

    def _get_returns(self, algorithm, symbol):

        history = algorithm.history([symbol], self.period, Resolution.DAILY)
        if history.empty: return None

        window = RollingWindow[float](self.period)
        rate_of_change = RateOfChange(1)

        def roc_updated(s, item):
            window.add(item.value)

        rate_of_change.updated += roc_updated

        history = history.close.reset_index(level=0, drop=True).iteritems()

        for time, value in history:
            rate_of_change.update(time, value)

        return [ x for x in window]
