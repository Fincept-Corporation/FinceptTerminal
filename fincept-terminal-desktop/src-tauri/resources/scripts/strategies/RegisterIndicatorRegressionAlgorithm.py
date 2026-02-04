# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0D15187F
# Category: Indicators
# Description: Regression algorithm reproducing data type bugs in the RegisterIndicator API. Related to GH 4205
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from CustomDataRegressionAlgorithm import Bitcoin

### <summary>
### Regression algorithm reproducing data type bugs in the RegisterIndicator API. Related to GH 4205.
### </summary>
class RegisterIndicatorRegressionAlgorithm(QCAlgorithm):
    # Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.
    def initialize(self):
        self.set_start_date(2020, 1, 5)
        self.set_end_date(2020, 1, 10)

        SP500 = Symbol.create(Futures.Indices.SP_500_E_MINI, SecurityType.FUTURE, Market.CME)
        self._symbol = _symbol = self.future_chain_provider.get_future_contract_list(SP500, (self.start_date + timedelta(days=1)))[0]
        self.add_future_contract(_symbol)

        # this collection will hold all indicators and at the end of the algorithm we will assert that all of them are ready
        self._indicators = []

        # this collection will be used to determine if the Selectors were called, we will assert so at the end of algorithm
        self._selector_called = [ False, False, False, False, False, False ]

        # First we will test that we can register our custom indicator using a QuoteBar consolidator
        indicator = CustomIndicator()
        consolidator = self.resolve_consolidator(_symbol, Resolution.MINUTE, QuoteBar)
        self.register_indicator(_symbol, indicator, consolidator)
        self._indicators.append(indicator)

        indicator2 = CustomIndicator()
        # We use the TimeDelta overload to fetch the consolidator
        consolidator = self.resolve_consolidator(_symbol, timedelta(minutes=1), QuoteBar)
        # We specify a custom selector to be used
        self.register_indicator(_symbol, indicator2, consolidator, lambda bar: self.set_selector_called(0) and bar)
        self._indicators.append(indicator2)

        # We use a IndicatorBase<IndicatorDataPoint> with QuoteBar data and a custom selector
        indicator3 = SimpleMovingAverage(10)
        consolidator = self.resolve_consolidator(_symbol, timedelta(minutes=1), QuoteBar)
        self.register_indicator(_symbol, indicator3, consolidator, lambda bar: self.set_selector_called(1) and (bar.ask.high - bar.bid.low))
        self._indicators.append(indicator3)

        # We test default consolidator resolution works correctly
        moving_average = SimpleMovingAverage(10)
        # Using Resolution, specifying custom selector and explicitly using TradeBar.volume
        self.register_indicator(_symbol, moving_average, Resolution.MINUTE, lambda bar: self.set_selector_called(2) and bar.volume)
        self._indicators.append(moving_average)

        moving_average2 = SimpleMovingAverage(10)
        # Using Resolution
        self.register_indicator(_symbol, moving_average2, Resolution.MINUTE)
        self._indicators.append(moving_average2)

        moving_average3 = SimpleMovingAverage(10)
        # Using timedelta
        self.register_indicator(_symbol, moving_average3, timedelta(minutes=1))
        self._indicators.append(moving_average3)

        moving_average4 = SimpleMovingAverage(10)
        # Using time_delta, specifying custom selector and explicitly using TradeBar.volume
        self.register_indicator(_symbol, moving_average4, timedelta(minutes=1), lambda bar: self.set_selector_called(3) and bar.volume)
        self._indicators.append(moving_average4)

        # Test custom data is able to register correctly and indicators updated
        symbol_custom = self.add_data(Bitcoin, "BTC", Resolution.MINUTE).symbol

        sma_custom_data = SimpleMovingAverage(1)
        self.register_indicator(symbol_custom, sma_custom_data, timedelta(minutes=1), lambda bar: self.set_selector_called(4) and bar.volume)
        self._indicators.append(sma_custom_data)

        sma_custom_data2 = SimpleMovingAverage(1)
        self.register_indicator(symbol_custom, sma_custom_data2, Resolution.MINUTE)
        self._indicators.append(sma_custom_data2)

        sma_custom_data3 = SimpleMovingAverage(1)
        consolidator = self.resolve_consolidator(symbol_custom, timedelta(minutes=1))
        self.register_indicator(symbol_custom, sma_custom_data3, consolidator, lambda bar: self.set_selector_called(5) and bar.volume)
        self._indicators.append(sma_custom_data3)

    def set_selector_called(self, position):
        self._selector_called[position] = True
        return True

    # OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    def on_data(self, data):
        if not self.portfolio.invested:
           self.set_holdings(self._symbol, 0.5)

    def on_end_of_algorithm(self):
        if any(not was_called for was_called in self._selector_called):
            raise ValueError("All selectors should of been called")
        if any(not indicator.is_ready for indicator in self._indicators):
            raise ValueError("All indicators should be ready")
        self.log(f'Total of {len(self._indicators)} are ready')

class CustomIndicator(PythonIndicator):
    def __init__(self):
        super().__init__()
        self.name = "Jose"
        self.value = 0

    def update(self, input):
        self.value = input.ask.high
        return True
