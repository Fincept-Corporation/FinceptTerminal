# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-6A2A6B6A
# Category: General Strategy
# Description: This algorithm showcases some features of the IObjectStore feature.
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from io import StringIO

class ObjectStoreExampleAlgorithm(QCAlgorithm):
    '''This algorithm showcases some features of the IObjectStore feature.
    One use case is to make consecutive backtests run faster by caching the results of
    potentially time consuming operations. In this example, we save the results of a
    history call. This pattern can be equally applied to a machine learning model being
    trained and then saving the model weights in the object store.
    '''
    spy_close_object_store_key = "spy_close"
    spy_close_history = RollingWindow[IndicatorDataPoint](252)
    spy_close_ema10_history = RollingWindow[IndicatorDataPoint](252)
    spy_close_ema50_history = RollingWindow[IndicatorDataPoint](252)

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

        self.SPY = self.add_equity("SPY", Resolution.MINUTE).symbol

        self.spy_close = self.identity(self.SPY, Resolution.DAILY)
        self.spy_close_ema10 = IndicatorExtensions.ema(self.spy_close, 10)
        self.spy_close_ema50 = IndicatorExtensions.ema(self.spy_close, 50)

        # track last year of close and EMA10/EMA50
        self.spy_close.updated += lambda _, args: self.spy_close_history.add(args)
        self.spy_close_ema10.updated += lambda _, args: self.spy_close_ema10_history.add(args)
        self.spy_close_ema50.updated += lambda _, args: self.spy_close_ema50_history.add(args)

        if self.object_store.contains_key(self.spy_close_object_store_key):
            # our object store has our historical data saved, read the data
            # and push it through the indicators to warm everything up
            values = self.object_store.read(self.spy_close_object_store_key)
            self.debug(f'{self.spy_close_object_store_key} key exists in object store.')

            history = pd.read_csv(StringIO(values), header=None, index_col=0, squeeze=True)
            history.index = pd.to_datetime(history.index)
            for time, close in history.items():
                self.spy_close.update(time, close)

        else:
            self.debug(f'{self.spy_close_object_store_key} key does not exist in object store. Fetching history...')

            # if our object store doesn't have our data, fetch the history to initialize
            # we're pulling the last year's worth of SPY daily trade bars to fee into our indicators
            history = self.history(self.SPY, timedelta(365), Resolution.DAILY).close.unstack(0).squeeze()

            for time, close in history.items():
                self.spy_close.update(time, close)

            # save our warm up data so next time we don't need to issue the history request
            self.object_store.save(self.spy_close_object_store_key,
                '\n'.join(reversed([f'{x.end_time},{x.value}' for x in self.spy_close_history])))

            # Can also use ObjectStore.save_bytes(key, byte[])
            # and to read  ObjectStore.read_bytes(key) => byte[]

            # we can also get a file path for our data. some ML libraries require model
            # weights to be loaded directly from a file path. The object store can provide
            # a file path for any key by: ObjectStore.get_file_path(key) => string (file path)

    def on_data(self, slice):

        close = self.spy_close
        ema10 = self.spy_close_ema10
        ema50 = self.spy_close_ema50

        if ema10 > close and ema10 > ema50:
            self.set_holdings(self.SPY, 1)

        elif ema10 < close and ema10 < ema50:
            self.set_holdings(self.SPY, -1)

        elif ema10 < ema50 and self.portfolio[self.SPY].is_long:
            self.liquidate(self.SPY)

        elif ema10 > ema50 and self.portfolio[self.SPY].is_short:
            self.liquidate(self.SPY)
