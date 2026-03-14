# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-95DFEFEB
# Category: Regression Test
# Description: This regression algorithm is expected to fail and verifies that a training event created in Initialize will get run A...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from time import sleep

### <summary>
### This regression algorithm is expected to fail and verifies that a training event
### created in Initialize will get run AND it will cause the algorithm to fail if it
### exceeds the "algorithm-manager-time-loop-maximum" config value, which the regression
### test sets to 0.5 minutes.
### </summary>
class TrainingInitializeRegressionAlgorithm(QCAlgorithm):
    '''Example algorithm showing how to use QCAlgorithm.train method'''

    def initialize(self):

        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

        self.add_equity("SPY", Resolution.DAILY)

        # this should cause the algorithm to fail
        # the regression test sets the time limit to 30 seconds and there's one extra
        # minute in the bucket, so a two minute sleep should result in RuntimeError
        self.train(lambda: sleep(150))

        # DateRules.tomorrow combined with TimeRules.midnight enforces that this event schedule will
        # have exactly one time, which will fire between the first data point and the next day at
        # midnight. So after the first data point, it will run this event and sleep long enough to
        # exceed the static max algorithm time loop time and begin to consume from the leaky bucket
        # the regression test sets the "algorithm-manager-time-loop-maximum" value to 30 seconds
        self.train(self.date_rules.tomorrow, self.time_rules.midnight, lambda: sleep(60))
                    # this will consume the single 'minute' available in the leaky bucket
                    # and the regression test will confirm that the leaky bucket is empty
