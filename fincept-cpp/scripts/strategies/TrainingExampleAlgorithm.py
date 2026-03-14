# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-4F56A3E5
# Category: General Strategy
# Description: Example algorithm showing how to use QCAlgorithm.train method
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from time import sleep

### <summary>
### Example algorithm showing how to use QCAlgorithm.train method
### </summary>
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="training" />
class TrainingExampleAlgorithm(QCAlgorithm):
    '''Example algorithm showing how to use QCAlgorithm.train method'''

    def initialize(self):

        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 14)

        self.add_equity("SPY", Resolution.DAILY)

        # Set TrainingMethod to be executed immediately
        self.train(self.training_method)

        # Set TrainingMethod to be executed at 8:00 am every Sunday
        self.train(self.date_rules.every(DayOfWeek.SUNDAY), self.time_rules.at(8 , 0), self.training_method)

    def training_method(self):

        self.log(f'Start training at {self.time}')
        # Use the historical data to train the machine learning model
        history = self.history(["SPY"], 200, Resolution.DAILY)

        # ML code:
        pass
