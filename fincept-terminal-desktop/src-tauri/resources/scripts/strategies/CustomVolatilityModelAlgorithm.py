# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-57E3FCCE
# Category: General Strategy
# Description: Example of custom volatility model
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Example of custom volatility model 
### </summary>
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="reality modelling" />
class CustomVolatilityModelAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2015,7,15)     #Set End Date
        self.set_cash(100000)           #Set Strategy Cash
        # Fincept Terminal Strategy Engine - Symbol Configuration
        self.equity = self.add_equity("SPY", Resolution.DAILY)
        self.equity.set_volatility_model(CustomVolatilityModel(10))


    def on_data(self, data):
        if not self.portfolio.invested and self.equity.volatility_model.volatility > 0:
            self.set_holdings("SPY", 1)


# Python implementation of StandardDeviationOfReturnsVolatilityModel
# Computes the annualized sample standard deviation of daily returns as the volatility of the security
# https://github.com/QuantConnect/Lean/blob/master/Common/Securities/Volatility/StandardDeviationOfReturnsVolatilityModel.cs
class CustomVolatilityModel():
    def __init__(self, periods):
        self.last_update = datetime.min
        self.last_price = 0
        self.needs_update = False
        self.period_span = timedelta(1)
        self.window = RollingWindow[float](periods)

        # Volatility is a mandatory attribute
        self.volatility = 0

    # Updates this model using the new price information in the specified security instance
    # Update is a mandatory method
    def update(self, security, data):
        time_since_last_update = data.end_time - self.last_update
        if time_since_last_update >= self.period_span and data.price > 0:
            if self.last_price > 0:
                self.window.add(float(data.price / self.last_price) - 1.0)
                self.needs_update = self.window.is_ready
            self.last_update = data.end_time
            self.last_price = data.price

        if self.window.count < 2:
            self.volatility = 0
            return

        if self.needs_update:
            self.needs_update = False
            std = np.std([ x for x in self.window ])
            self.volatility = std * np.sqrt(252.0)

    # Returns history requirements for the volatility model expressed in the form of history request
    # GetHistoryRequirements is a mandatory method
    def get_history_requirements(self, security, utc_time):
        # For simplicity's sake, we will not set a history requirement 
        return None
