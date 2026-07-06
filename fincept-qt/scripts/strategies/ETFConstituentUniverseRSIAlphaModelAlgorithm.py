# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-4249AE56
# Category: ETF
# Description: Example algorithm demonstrating the usage of the RSI indicator in combination with ETF constituents data to replicate...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example algorithm demonstrating the usage of the RSI indicator
### in combination with ETF constituents data to replicate the weighting
### of the ETF's assets in our own account.
### </summary>
class ETFConstituentUniverseRSIAlphaModelAlgorithm(QCAlgorithm):
    ### <summary>
    ### Initialize the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    ### </summary>
    def initialize(self):
        self.set_start_date(2020, 12, 1)
        self.set_end_date(2021, 1, 31)
        self.set_cash(100000)

        self.set_alpha(ConstituentWeightedRsiAlphaModel())
        self.set_portfolio_construction(InsightWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())

        spy = self.add_equity("SPY", Resolution.HOUR).symbol

        # We load hourly data for ETF constituents in this algorithm
        self.universe_settings.resolution = Resolution.HOUR
        self.settings.minimum_order_margin_portfolio_percentage = 0.01

        self.add_universe(self.universe.etf(spy, self.universe_settings, self.filter_etf_constituents))

    ### <summary>
    ### Filters ETF constituents
    ### </summary>
    ### <param name="constituents">ETF constituents</param>
    ### <returns>ETF constituent Symbols that we want to include in the algorithm</returns>
    def filter_etf_constituents(self, constituents):
        return [i.symbol for i in constituents if i.weight is not None and i.weight >= 0.001]


### <summary>
### Alpha model making use of the RSI indicator and ETF constituent weighting to determine
### which assets we should invest in and the direction of investment
### </summary>
class ConstituentWeightedRsiAlphaModel(AlphaModel):
    def __init__(self, max_trades=None):
        self.rsi_symbol_data = {}

    def update(self, algorithm: QCAlgorithm, data: Slice):
        algo_constituents = []
        for bar_symbol in data.bars.keys():
            if not algorithm.securities[bar_symbol].cache.has_data(ETFConstituentUniverse):
                continue

            constituent_data = algorithm.securities[bar_symbol].cache.get_data[ETFConstituentUniverse]()
            algo_constituents.append(constituent_data)

        if len(algo_constituents) == 0 or len(data.bars) == 0:
            # Don't do anything if we have no data we can work with
            return []

        constituents = {i.symbol:i for i in algo_constituents}

        for bar in data.bars.values():
            if bar.symbol not in constituents:
                # Dealing with a manually added equity, which in this case is SPY
                continue

            if bar.symbol not in self.rsi_symbol_data:
                # First time we're initializing the RSI.
                # It won't be ready now, but it will be
                # after 7 data points
                constituent = constituents[bar.symbol]
                self.rsi_symbol_data[bar.symbol] = SymbolData(bar.symbol, algorithm, constituent, 7)

        all_ready = all([sd.rsi.is_ready for sd in self.rsi_symbol_data.values()])
        if not all_ready:
            # We're still warming up the RSI indicators.
            return []

        insights = []

        for symbol, symbol_data in self.rsi_symbol_data.items():
            average_loss = symbol_data.rsi.average_loss.current.value
            average_gain = symbol_data.rsi.average_gain.current.value

            # If we've lost more than gained, then we think it's going to go down more
            direction = InsightDirection.DOWN if average_loss > average_gain else InsightDirection.UP

            # Set the weight of the insight as the weight of the ETF's
            # holding. The InsightWeightingPortfolioConstructionModel
            # will rebalance our portfolio to have the same percentage
            # of holdings in our algorithm that the ETF has.
            insights.append(Insight.price(
                symbol,
                timedelta(days=1),
                direction,
                float(average_loss if direction == InsightDirection.DOWN else average_gain),
                weight=float(symbol_data.constituent.weight)
            ))
        
        return insights


class SymbolData:
    def __init__(self, symbol, algorithm, constituent, period):
        self.symbol = symbol
        self.constituent = constituent
        self.rsi = algorithm.rsi(symbol, period, MovingAverageType.EXPONENTIAL, Resolution.HOUR)
