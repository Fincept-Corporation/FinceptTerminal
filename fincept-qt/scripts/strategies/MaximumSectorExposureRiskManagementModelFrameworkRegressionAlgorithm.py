# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-C143732F
# Category: Risk Management
# Description: Regression algorithm to assert the behavior of <see cref="MaximumSectorExposureRiskManagementModel"/>
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from BaseFrameworkRegressionAlgorithm import BaseFrameworkRegressionAlgorithm
from Risk.MaximumSectorExposureRiskManagementModel import MaximumSectorExposureRiskManagementModel

### <summary>
### Regression algorithm to assert the behavior of <see cref="MaximumSectorExposureRiskManagementModel"/>.
### </summary>
class MaximumSectorExposureRiskManagementModelFrameworkRegressionAlgorithm(BaseFrameworkRegressionAlgorithm):

    def initialize(self):
        super().initialize()
        # Set requested data resolution
        self.universe_settings.resolution = Resolution.DAILY

        self.set_start_date(2014, 2, 1)  #Set Start Date
        self.set_end_date(2014, 5, 1)    #Set End Date

        # set algorithm framework models
        tickers = [ "AAPL", "MSFT", "GOOG", "AIG", "BAC" ]
        self.set_universe_selection(FineFundamentalUniverseSelectionModel(
            lambda coarse: [ x.symbol for x in coarse if x.symbol.value in tickers ],
            lambda fine: [ x.symbol for x in fine ]
        ))

        # define risk management model such that maximum weight of a single sector be 10%
        # Number of of trades changed from 34 to 30 when using the MaximumSectorExposureRiskManagementModel
        self.set_risk_management(MaximumSectorExposureRiskManagementModel(0.1))

    def on_end_of_algorithm(self):
        pass
