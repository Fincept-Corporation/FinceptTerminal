# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-0021591C
# Category: Indicators
# Description: The algorithm creates new indicator value with the existing indicator method by Indicator Extensions Demonstration of...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from HistoryAlgorithm import *

### <summary>
### The algorithm creates new indicator value with the existing indicator method by Indicator Extensions
### Demonstration of using the external custom data to request the IBM and SPY daily data
### </summary>
### <meta name="tag" content="using data" />
### <meta name="tag" content="using quantconnect" />
### <meta name="tag" content="custom data" />
### <meta name="tag" content="indicators" />
### <meta name="tag" content="indicator classes" />
### <meta name="tag" content="plotting indicators" />
### <meta name="tag" content="charting" />
class CustomDataIndicatorExtensionsAlgorithm(QCAlgorithm):

    # Initialize the data and resolution you require for your strategy
    def initialize(self):

        self.set_start_date(2014,1,1) 
        self.set_end_date(2018,1,1)  
        self.set_cash(25000)
        
        self.ibm = 'IBM'
        self.spy = 'SPY'
        
        # Define the symbol and "type" of our generic data
        self.add_data(CustomDataEquity, self.ibm, Resolution.DAILY)
        self.add_data(CustomDataEquity, self.spy, Resolution.DAILY)
        
        # Set up default Indicators, these are just 'identities' of the closing price
        self.ibm_sma = self.sma(self.ibm, 1, Resolution.DAILY)
        self.spy_sma = self.sma(self.spy, 1, Resolution.DAILY)
        
        # This will create a new indicator whose value is sma_s_p_y / sma_i_b_m
        self.ratio = IndicatorExtensions.over(self.spy_sma, self.ibm_sma)
        
        # Plot indicators each time they update using the PlotIndicator function
        self.plot_indicator("Ratio", self.ratio)
        self.plot_indicator("Data", self.ibm_sma, self.spy_sma)
    
    # OnData event is the primary entry point for your algorithm. Each new data point will be pumped in here.
    def on_data(self, data):
        
        # Wait for all indicators to fully initialize
        if not (self.ibm_sma.is_ready and self.spy_sma.is_ready and self.ratio.is_ready): return
        if not self.portfolio.invested and self.ratio.current.value > 1:
            self.market_order(self.ibm, 100)
        elif self.ratio.current.value < 1:
                self.liquidate()
