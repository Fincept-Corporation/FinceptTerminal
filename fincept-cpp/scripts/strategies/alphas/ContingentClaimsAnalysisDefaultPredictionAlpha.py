# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-70CBDC3D
# Category: Alpha Model
# Description: Contingent Claim Analysis is put forth by Robert Merton, recepient of the Noble Prize in Economics in 1997 for his wo...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

import scipy.stats as sp
from Risk.NullRiskManagementModel import NullRiskManagementModel
from Portfolio.EqualWeightingPortfolioConstructionModel import EqualWeightingPortfolioConstructionModel
from Execution.ImmediateExecutionModel import ImmediateExecutionModel

class ContingentClaimsAnalysisDefaultPredictionAlpha(QCAlgorithm):
    ''' Contingent Claim Analysis is put forth by Robert Merton, recepient of the Noble Prize in Economics in 1997 for his work in contributing to
    Black-Scholes option pricing theory, which says that the equity market value of stockholders’ equity is given by the Black-Scholes solution
    for a European call option. This equation takes into account Debt, which in CCA is the equivalent to a strike price in the BS solution. The probability
    of default on corporate debt can be calculated as the N(-d2) term, where d2 is a function of the interest rate on debt(µ), face value of the debt (B), value of the firm's assets (V),
    standard deviation of the change in a firm's asset value (σ), the dividend and interest payouts due (D), and the time to maturity of the firm's debt(τ). N(*) is the cumulative
    distribution function of a standard normal distribution, and calculating N(-d2) gives us the probability of the firm's assets being worth less
    than the debt of the company at the time that the debt reaches maturity -- that is, the firm doesn't have enough in assets to pay off its debt and defaults.

    We use a Fine/Coarse Universe Selection model to select small cap stocks, who we postulate are more likely to default
    on debt in general than blue-chip companies, and extract Fundamental data to plug into the CCA formula.
    This Alpha emits insights based on whether or not a company is likely to default given its probability of default vs a default probability threshold that we set arbitrarily.

    Prob. default (on principal B at maturity T) = Prob(VT < B) = 1 - N(d2) = N(-d2) where -d2(µ) = -{ln(V/B) + [(µ - D) - ½σ2]τ}/ σ √τ.
        N(d) = (univariate) cumulative standard normal distribution function (from -inf to d)
        B = face value (principal) of the debt
        D = dividend + interest payout
        V = value of firm’s assets
        σ (sigma) = standard deviation of firm value changes (returns in V)
        τ (tau)  = time to debt’s maturity
        µ (mu) = interest rate

    This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open
    sourced so the community and client funds can see an example of an alpha.'''

    def initialize(self):

        ## Set requested data resolution and variables to help with Universe Selection control
        self.universe_settings.resolution = Resolution.DAILY
        self.month = -1

        ## Declare single variable to be passed in multiple places -- prevents issue with conflicting start dates declared in different places
        self.set_start_date(2018,1,1)
        self.set_cash(100000)

        ## SPDR Small Cap ETF is a better benchmark than the default SP500
        self.set_benchmark('IJR')

        ## Set Universe Selection Model
        self.set_universe_selection(FineFundamentalUniverseSelectionModel(self.coarse_selection_function, self.fine_selection_function))
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))
        ## Set CCA Alpha Model
        self.set_alpha(ContingentClaimsAnalysisAlphaModel())

        ## Set Portfolio Construction Model
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())

        ## Set Execution Model
        self.set_execution(ImmediateExecutionModel())

        ## Set Risk Management Model
        self.set_risk_management(NullRiskManagementModel())


    def coarse_selection_function(self, coarse):
        ## Boolean controls so that our symbol universe is only updated once per month
        if self.time.month == self.month:
            return Universe.UNCHANGED
        self.month = self.time.month

        ## Sort by dollar volume, lowest to highest
        sorted_by_dollar_volume = sorted([x for x in coarse if x.has_fundamental_data],
            key=lambda x: x.dollar_volume, reverse=True)

        ## Return smallest 750 -- idea is that smaller companies are most likely to go bankrupt than blue-chip companies
        ## Filter for assets with fundamental data
        return [x.symbol for x in sorted_by_dollar_volume[:750]]

    def fine_selection_function(self, fine):

            def is_valid(x):
                statement = x.financial_statements
                sheet = statement.balance_sheet
                total_assets = sheet.total_assets
                ratios = x.operation_ratios

                return total_assets.one_month > 0 and \
                        total_assets.three_months > 0 and \
                        total_assets.six_months  > 0 and \
                        total_assets.twelve_months > 0 and \
                        sheet.current_liabilities.twelve_months > 0 and \
                        sheet.interest_payable.twelve_months > 0 and \
                        ratios.total_assets_growth.one_year > 0 and \
                        statement.income_statement.gross_dividend_payment.twelve_months > 0 and \
                        ratios.roa.one_year > 0

            return [x.symbol for x in sorted(fine, key=lambda x: is_valid(x))]


class ContingentClaimsAnalysisAlphaModel:

    def __init__(self, *args, **kwargs):
        self.probability_of_default_by_symbol = {}
        self.default_threshold = kwargs['default_threshold'] if 'default_threshold' in kwargs else 0.25

    def update(self, algorithm, data):
        '''Updates this alpha model with the latest data from the algorithm.
        This is called each time the algorithm receives data for subscribed securities
        Args:
            algorithm: The algorithm instance
            data: The new data available
        Returns:
            The new insights generated'''

        ## Build a list to hold our insights
        insights = []

        for symbol, pod in self.probability_of_default_by_symbol.items():

            ## If Prob. of Default is greater than our set threshold, then emit an insight indicating that this asset is trending downward
            if pod >= self.default_threshold and pod != 1.0:
                insights.append(Insight.price(symbol, timedelta(30), InsightDirection.DOWN, pod, None))

        return insights

    def on_securities_changed(self, algorithm, changes):

        for removed in changes.removed_securities:
            self.probability_of_default_by_symbol.pop(removed.symbol, None)

        # initialize data for added securities
        symbols = [ x.symbol for x in changes.added_securities ]

        for symbol in symbols:
            if symbol not in self.probability_of_default_by_symbol:
                ## CCA valuation
                pod = self.get_probability_of_default(algorithm, symbol)
                if pod is not None:
                    self.probability_of_default_by_symbol[symbol] = pod

    def get_probability_of_default(self, algorithm, symbol):
        '''This model applies options pricing theory, Black-Scholes specifically,
        to fundamental data to give the probability of a default'''
        security = algorithm.securities[symbol]
        if security.fundamentals is None or security.fundamentals.financial_statements is None or security.fundamentals.operation_ratios is None:
            return None

        statement = security.fundamentals.financial_statements
        sheet = statement.balance_sheet
        total_assets = sheet.total_assets

        tau = 360   ## Days
        mu = security.fundamentals.operation_ratios.roa.one_year
        V = total_assets.twelve_months
        B = sheet.current_liabilities.twelve_months
        D = statement.income_statement.gross_dividend_payment.twelve_months + sheet.interest_payable.twelve_months

        series = pd.Series(
            [
                total_assets.one_month,
                total_assets.three_months,
                total_assets.six_months,
                V
            ])
        sigma = series.iloc[series.nonzero()[0]]
        sigma = np.std(sigma.pct_change()[1:len(sigma)])

        d2 = ((np.log(V) - np.log(B)) + ((mu - D) - 0.5*sigma**2.0)*tau)/ (sigma*np.sqrt(tau))
        return sp.norm.cdf(-d2)
