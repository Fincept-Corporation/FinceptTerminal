# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9D3D6D8F
# Category: Universe Selection
# Description: Provides an example where WarmUpIndicator method is used to warm up indicators
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class SmaCrossUniverseSelectionAlgorithm(QCAlgorithm):
    '''Provides an example where WarmUpIndicator method is used to warm up indicators
    after their security is added and before (Universe Selection scenario)'''

    count = 10
    tolerance = 0.01
    target_percent = 1 / count
    averages = dict()

    def initialize(self):

        self.universe_settings.leverage = 2
        self.universe_settings.resolution = Resolution.DAILY

        self.set_start_date(2018, 1, 1)
        self.set_end_date(2019, 1, 1)
        self.set_cash(1000000)

        self.settings.automatic_indicator_warm_up = True

        ibm = self.add_equity("IBM", Resolution.HOUR).symbol
        ibm_sma = self.sma(ibm, 40)
        self.log(f"{ibm_sma.name}: {ibm_sma.current.time} - {ibm_sma}. IsReady? {ibm_sma.is_ready}")

        spy = self.add_equity("SPY", Resolution.HOUR).symbol
        spy_sma = self.sma(spy, 10)     # Data point indicator
        spy_atr = self.atr(spy, 10,)    # Bar indicator
        spy_vwap = self.vwap(spy, 10)   # TradeBar indicator
        self.log(f"SPY    - Is ready? SMA: {spy_sma.is_ready}, ATR: {spy_atr.is_ready}, VWAP: {spy_vwap.is_ready}")

        eur = self.add_forex("EURUSD", Resolution.HOUR).symbol
        eur_sma = self.sma(eur, 20, Resolution.DAILY)
        eur_atr = self.atr(eur, 20, MovingAverageType.SIMPLE, Resolution.DAILY)
        self.log(f"EURUSD - Is ready? SMA: {eur_sma.is_ready}, ATR: {eur_atr.is_ready}")

        self.add_universe(self.coarse_sma_selector)

        # Since the indicators are ready, we will receive error messages
        # reporting that the algorithm manager is trying to add old information
        self.set_warm_up(10)

    def coarse_sma_selector(self, coarse):

        score = dict()
        for cf in coarse:
            if not cf.has_fundamental_data:
               continue
            symbol = cf.symbol
            price = cf.adjusted_price
            # grab the SMA instance for this symbol
            avg = self.averages.setdefault(symbol, SimpleMovingAverage(100))
            self.warm_up_indicator(symbol, avg, Resolution.DAILY)
            # Update returns true when the indicators are ready, so don't accept until they are
            if avg.update(cf.end_time, price):
               value = avg.current.value
               # only pick symbols who have their price over their 100 day sma
               if value > price * self.tolerance:
                    score[symbol] = (value - price) / ((value + price) / 2)

        # prefer symbols with a larger delta by percentage between the two averages
        sorted_score = sorted(score.items(), key=lambda kvp: kvp[1], reverse=True)
        return [x[0] for x in sorted_score[:self.count]]

    def on_securities_changed(self, changes):
        for security in changes.removed_securities:
            if security.invested:
                self.liquidate(security.symbol)

        for security in changes.added_securities:
            self.set_holdings(security.symbol, self.target_percent)
