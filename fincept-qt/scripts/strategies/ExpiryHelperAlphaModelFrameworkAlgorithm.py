# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

from AlgorithmImports import *

### <summary>
### Expiry Helper algorithm uses Expiry helper class in an Alpha Model
### </summary>
class ExpiryHelperAlphaModelFrameworkAlgorithm(QCAlgorithm):
    '''Expiry Helper framework algorithm uses Expiry helper class in an Alpha Model'''

    def initialize(self):
        ''' Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        # Set requested data resolution
        self.universe_settings.resolution = Resolution.HOUR

        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2014,1,1)      #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        symbols = [ Symbol.create("SPY", SecurityType.EQUITY, Market.USA) ]

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))
        self.set_alpha(self.ExpiryHelperAlphaModel())
        self.set_portfolio_construction(EqualWeightingPortfolioConstructionModel())
        self.set_execution(ImmediateExecutionModel())
        self.set_risk_management(MaximumDrawdownPercentPerSecurity(0.01))

        self.insights_generated += self.on_insights_generated

    def on_insights_generated(self, s, e):
        for insight in e.insights:
            self.log(f"{e.date_time_utc.isoweekday()}: Close Time {insight.close_time_utc} {insight.close_time_utc.isoweekday()}")

    class ExpiryHelperAlphaModel(AlphaModel):
        next_update = None
        direction = InsightDirection.UP

        def update(self, algorithm, data):

            if self.next_update is not None and self.next_update > algorithm.time:
                return []

            expiry = Expiry.END_OF_DAY

            # Use the Expiry helper to calculate a date/time in the future
            self.next_update = expiry(algorithm.time)

            weekday = algorithm.time.isoweekday()

            insights = []
            for symbol in data.bars.keys():
                # Expected CloseTime: next month on the same day and time
                if weekday == 1:
                    insights.append(Insight.price(symbol, Expiry.ONE_MONTH, self.direction))
                # Expected CloseTime: next month on the 1st at market open time
                elif weekday == 2:
                    insights.append(Insight.price(symbol, Expiry.END_OF_MONTH, self.direction))
                # Expected CloseTime: next Monday at market open time
                elif weekday == 3:
                    insights.append(Insight.price(symbol, Expiry.END_OF_WEEK, self.direction))
                # Expected CloseTime: next day (Friday) at market open time
                elif weekday == 4:
                    insights.append(Insight.price(symbol, Expiry.END_OF_DAY, self.direction))

            return insights
