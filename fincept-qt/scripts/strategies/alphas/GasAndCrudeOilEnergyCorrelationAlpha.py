# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9D7FFCA4
# Category: Alpha Model
# Description: Energy prices, especially Oil and Natural Gas, are in general fairly correlated,
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
'''
    Energy prices, especially Oil and Natural Gas, are in general fairly correlated,
    meaning they typically move in the same direction as an overall trend. This Alpha
    uses this idea and implements an Alpha Model that takes Natural Gas ETF price
    movements as a leading indicator for Crude Oil ETF price movements. We take the
    Natural Gas/Crude Oil ETF pair with the highest historical price correlation and
    then create insights for Crude Oil depending on whether or not the Natural Gas ETF price change
    is above/below a certain threshold that we set (arbitrarily).



    This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open
    sourced so the community and client funds can see an example of an alpha.
'''

from AlgorithmImports import *

class GasAndCrudeOilEnergyCorrelationAlpha(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2018, 1, 1)   #Set Start Date
        self.set_cash(100000)            #Set Strategy Cash

        natural_gas = [Symbol.create(x, SecurityType.EQUITY, Market.USA) for x in ['UNG','BOIL','FCG']]
        crude_oil = [Symbol.create(x, SecurityType.EQUITY, Market.USA) for x in ['USO','UCO','DBO']]

        ## Set Universe Selection
        self.universe_settings.resolution = Resolution.MINUTE
        self.set_universe_selection( ManualUniverseSelectionModel(natural_gas + crude_oil) )
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))

        ## Custom Alpha Model
        self.set_alpha(PairsAlphaModel(leading = natural_gas, following = crude_oil, history_days = 90, resolution = Resolution.MINUTE))

        ## Equal-weight our positions, in this case 100% in USO
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel(resolution = Resolution.MINUTE))

        ## Immediate Execution Fill Model
        self.set_execution(CustomExecutionModel())

        ## Null Risk-Management Model
        self.set_risk_management(NullRiskManagementModel())

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            self.debug(f'Purchased Stock: {order_event.symbol}')

    def on_end_of_algorithm(self):
        for kvp in self.portfolio:
            if kvp.value.invested:
                self.log(f'Invested in: {kvp.key}')


class PairsAlphaModel:
    '''This Alpha model assumes that the ETF for natural gas is a good leading-indicator
        of the price of the crude oil ETF. The model will take in arguments for a threshold
        at which the model triggers an insight, the length of the look-back period for evaluating
        rate-of-change of UNG prices, and the duration of the insight'''

    def __init__(self, *args, **kwargs):
        self.leading = kwargs.get('leading', [])
        self.following = kwargs.get('following', [])
        self.history_days = kwargs.get('history_days', 90) ## In days
        self.lookback = kwargs.get('lookback', 5)
        self.resolution = kwargs.get('resolution', Resolution.HOUR)
        self.prediction_interval = Time.multiply(Extensions.to_time_span(self.resolution), 5) ## Arbitrary
        self.difference_trigger = kwargs.get('difference_trigger', 0.75)
        self._symbol_data_by_symbol = {}
        self.next_update = None

    def update(self, algorithm, data):

        if (self.next_update is None) or (algorithm.time > self.next_update):
            self.correlation_pairs_selection()
            self.next_update = algorithm.time + timedelta(30)

        magnitude = round(self.pairs[0].rate_of_return / 100, 6)

        ## Check if Natural Gas returns are greater than the threshold we've set
        if self.pairs[0].rate_of_return > self.difference_trigger:
            return [Insight.price(self.pairs[1].symbol, self.prediction_interval, InsightDirection.UP, magnitude)]
        if self.pairs[0].rate_of_return < -self.difference_trigger:
            return [Insight.price(self.pairs[1].symbol, self.prediction_interval, InsightDirection.DOWN, magnitude)]

        return []

    def correlation_pairs_selection(self):
        ## Get returns for each natural gas/oil ETF
        daily_return = {}
        for symbol, symbol_data in self._symbol_data_by_symbol.items():
            daily_return[symbol] = symbol_data.daily_return_array

        ## Estimate coefficients of different correlation measures
        tau = pd.DataFrame.from_dict(daily_return).corr(method='kendall')

        ## Calculate the pair with highest historical correlation
        max_corr = -1
        for x in self.leading:
            df = tau[[x]].loc[self.following]
            corr = float(df.max())
            if corr > max_corr:
                self.pairs = (
                    self._symbol_data_by_symbol[x],
                    self._symbol_data_by_symbol[df.idxmax()[0]])
                max_corr = corr

    def on_securities_changed(self, algorithm, changes):
        '''Event fired each time the we add/remove securities from the data feed
        Args:
            algorithm: The algorithm instance that experienced the change in securities
            changes: The security additions and removals from the algorithm'''
        for removed in changes.removed_securities:
            symbol_data = self._symbol_data_by_symbol.pop(removed.symbol, None)
            if symbol_data is not None:
                symbol_data.remove_consolidators(algorithm)

        # initialize data for added securities
        symbols = [ x.symbol for x in changes.added_securities ]
        history = algorithm.history(symbols, self.history_days + 1, Resolution.DAILY)
        if history.empty: return

        tickers = history.index.levels[0]
        for ticker in tickers:
            symbol = SymbolCache.get_symbol(ticker)
            if symbol not in self._symbol_data_by_symbol:
                symbol_data = SymbolData(symbol, self.history_days, self.lookback, self.resolution, algorithm)
                self._symbol_data_by_symbol[symbol] = symbol_data
                symbol_data.update_daily_rate_of_change(history.loc[ticker])

        history = algorithm.history(symbols, self.lookback, self.resolution)
        if history.empty: return
        for ticker in tickers:
            symbol = SymbolCache.get_symbol(ticker)
            if symbol in self._symbol_data_by_symbol:
                self._symbol_data_by_symbol[symbol].update_rate_of_change(history.loc[ticker])

class SymbolData:
    '''Contains data specific to a symbol required by this model'''
    def __init__(self, symbol, daily_lookback, lookback, resolution, algorithm):
        self.symbol = symbol

        self.daily_return = RateOfChangePercent(f'{symbol}.daily_rocp({1})', 1)
        self.daily_consolidator = algorithm.resolve_consolidator(symbol, Resolution.DAILY)
        self.daily_return_history = RollingWindow[IndicatorDataPoint](daily_lookback)

        def updatedaily_return_history(s, e):
            self.daily_return_history.add(e)

        self.daily_return.updated += updatedaily_return_history
        algorithm.register_indicator(symbol, self.daily_return, self.daily_consolidator)

        self.rocp = RateOfChangePercent(f'{symbol}.rocp({lookback})', lookback)
        self.consolidator = algorithm.resolve_consolidator(symbol, resolution)
        algorithm.register_indicator(symbol, self.rocp, self.consolidator)

    def remove_consolidators(self, algorithm):
        algorithm.subscription_manager.remove_consolidator(self.symbol, self.consolidator)
        algorithm.subscription_manager.remove_consolidator(self.symbol, self.daily_consolidator)

    def update_rate_of_change(self, history):
        for tuple in history.itertuples():
            self.rocp.update(tuple.Index, tuple.close)

    def update_daily_rate_of_change(self, history):
        for tuple in history.itertuples():
            self.daily_return.update(tuple.Index, tuple.close)

    @property
    def rate_of_return(self):
        return float(self.rocp.current.value)

    @property
    def daily_return_array(self):
        return pd.Series({x.end_time: x.value for x in self.daily_return_history})

    def __repr__(self):
        return f"{self.rocp.name} - {Return}"


class CustomExecutionModel(ExecutionModel):
    '''Provides an implementation of IExecutionModel that immediately submits market orders to achieve the desired portfolio targets'''

    def __init__(self):
        '''Initializes a new instance of the ImmediateExecutionModel class'''
        self.targets_collection = PortfolioTargetCollection()
        self.previous_symbol = None

    def execute(self, algorithm, targets):
        '''Immediately submits orders for the specified portfolio targets.
        Args:
            algorithm: The algorithm instance
            targets: The portfolio targets to be ordered'''

        self.targets_collection.add_range(targets)

        for target in self.targets_collection.order_by_margin_impact(algorithm):
            open_quantity = sum([x.quantity for x in algorithm.transactions.get_open_orders(target.symbol)])
            existing = algorithm.securities[target.symbol].holdings.quantity + open_quantity
            quantity = target.quantity - existing
            ## Liquidate positions in Crude Oil ETF that is no longer part of the highest-correlation pair
            if (str(target.symbol) != str(self.previous_symbol)) and (self.previous_symbol is not None):
                algorithm.liquidate(self.previous_symbol)
            if quantity != 0:
                algorithm.market_order(target.symbol, quantity)
                self.previous_symbol = target.symbol
        self.targets_collection.clear_fulfilled(algorithm)
