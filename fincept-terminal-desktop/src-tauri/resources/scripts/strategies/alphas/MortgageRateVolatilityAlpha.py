# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-B86663BE
# Category: Alpha Model
# Description: This Alpha Model uses Wells Fargo 30-year Fixed Rate Mortgage data from Quandl to
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
'''
    This Alpha Model uses Wells Fargo 30-year Fixed Rate Mortgage data from Quandl to 
    generate Insights about the movement of Real Estate ETFs. Mortgage rates can provide information 
    regarding the general price trend of real estate, and ETFs provide good continuous-time instruments 
    to measure the impact against. Volatility in mortgage rates tends to put downward pressure on real 
    estate prices, whereas stable mortgage rates, regardless of true rate, lead to stable or higher real
    estate prices. This Alpha model seeks to take advantage of this correlation by emitting insights
    based on volatility and rate deviation from its historic mean.

    This alpha is part of the Benchmark Alpha Series created by QuantConnect which are open
    sourced so the community and client funds can see an example of an alpha.
'''

from AlgorithmImports import *

class MortgageRateVolatilityAlpha(QCAlgorithmFramework):

    def initialize(self):

        # Set requested data resolution
        self.set_start_date(2017, 1, 1)   #Set Start Date
        self.set_cash(100000)           #Set Strategy Cash

        self.universe_settings.resolution = Resolution.DAILY
        
        ## Universe of six liquid real estate ETFs
        etfs = ['VNQ', 'REET', 'TAO', 'FREL', 'SRET', 'HIPS']
        symbols = [ Symbol.create(etf, SecurityType.EQUITY, Market.USA) for etf in etfs ]
        self.set_security_initializer(lambda security: security.set_fee_model(ConstantFeeModel(0)))
        self.set_universe_selection(ManualUniverseSelectionModel(symbols) )
            
        self.set_alpha(MortgageRateVolatilityAlphaModel(self))
        
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel()) 
        
        self.set_execution(ImmediateExecutionModel())
        
        self.set_risk_management(NullRiskManagementModel())


class MortgageRateVolatilityAlphaModel(AlphaModel):
    
    def __init__(self, algorithm, indicator_period = 15, insight_magnitude = 0.005, deviations = 2):
        ## Add Quandl data for a Well's Fargo 30-year Fixed Rate mortgage
        self.mortgage_rate = algorithm.add_data(QuandlMortgagePriceColumns, 'WFC/PR_GOV_30YFIXEDVA_APR').symbol
        self.indicator_period = indicator_period
        self.insight_duration = TimeSpan.from_days(indicator_period)
        self.insight_magnitude = insight_magnitude
        self.deviations = deviations
        
        ## Add indicators for the mortgage rate -- Standard Deviation and Simple Moving Average
        self.mortgage_rate_std = algorithm.std(self.mortgage_rate, indicator_period)
        self.mortgage_rate_sma = algorithm.sma(self.mortgage_rate, indicator_period)
        
        ## Use a history call to warm-up the indicators
        self.warmup_indicators(algorithm)
    
    def update(self, algorithm, data):
        insights = []
        
        ## Return empty list if data slice doesn't contain monrtgage rate data
        if self.mortgage_rate not in data.keys():
            return []

        ## Extract current mortgage rate, the current STD indicator value, and current SMA value
        mortgage_rate = data[self.mortgage_rate].value
        deviation = self.deviations * self.mortgage_rate_std.current.value
        sma = self.mortgage_rate_sma.current.value
        
        ## If volatility in mortgage rates is high, then we emit an Insight to sell
        if (mortgage_rate < sma - deviation) or (mortgage_rate > sma + deviation):
            ## Emit insights for all securities that are currently in the Universe,
            ## except for the Quandl Symbol
            insights = [Insight(security, self.insight_duration, InsightType.PRICE, InsightDirection.DOWN, self.insight_magnitude, None) \
                        for security in algorithm.active_securities.keys if security != self.mortgage_rate]
        
        ## If volatility in mortgage rates is low, then we emit an Insight to buy
        if (mortgage_rate < sma - deviation/2) or (mortgage_rate > sma + deviation/2):
            insights = [Insight(security, self.insight_duration, InsightType.PRICE, InsightDirection.UP, self.insight_magnitude, None) \
                        for security in algorithm.active_securities.keys if security != self.mortgage_rate]
        
        return insights
    
    def warmup_indicators(self, algorithm):
        ## Make a history call and update the indicators
        history = algorithm.history(self.mortgage_rate, self.indicator_period, Resolution.DAILY)
        for index, row in history.iterrows():
            self.mortgage_rate_std.update(index[1], row['value'])
            self.mortgage_rate_sma.update(index[1], row['value'])
    
class QuandlMortgagePriceColumns(PythonQuandl):
    
    def __init__(self):
        ## Rename the Quandl object column to the data we want, which is the 'Value' column
        ## of the CSV that our API call returns
        self.value_column_name = "Value"
