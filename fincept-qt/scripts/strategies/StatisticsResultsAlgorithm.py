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
### Demonstration of how to access the statistics results from within an algorithm through the `Statistics` property.
### </summary>
class StatisticsResultsAlgorithm(QCAlgorithm):

    most_traded_security_statistic = "Most Traded Security"
    most_traded_security_trade_count_statistic = "Most Traded Security Trade Count"

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)
        self.set_cash(100000)

        self.spy = self.add_equity("SPY", Resolution.MINUTE).symbol
        self.ibm = self.add_equity("IBM", Resolution.MINUTE).symbol

        self.fast_spy_ema = self.ema(self.spy, 30, Resolution.MINUTE)
        self.slow_spy_ema = self.ema(self.spy, 60, Resolution.MINUTE)

        self.fast_ibm_ema = self.ema(self.spy, 10, Resolution.MINUTE)
        self.slow_ibm_ema = self.ema(self.spy, 30, Resolution.MINUTE)

        self.trade_counts = {self.spy: 0, self.ibm: 0}

    def on_data(self, data: Slice):
        if not self.slow_spy_ema.is_ready: return

        if self.fast_spy_ema > self.slow_spy_ema:
            self.set_holdings(self.spy, 0.5)
        elif self.securities[self.spy].invested:
            self.liquidate(self.spy)

        if self.fast_ibm_ema > self.slow_ibm_ema:
            self.set_holdings(self.ibm, 0.2)
        elif self.securities[self.ibm].invested:
            self.liquidate(self.ibm)

    def on_order_event(self, order_event):
        if order_event.status == OrderStatus.FILLED:
            # We can access the statistics summary at runtime
            statistics = self.statistics.summary
            statistics_str = "".join([f"{kvp.key}: {kvp.value}" for kvp in statistics])
            self.debug(f"Statistics after fill:{statistics_str}")

            # Access a single statistic
            self.log(f"Total trades so far: {statistics[PerformanceMetrics.TOTAL_ORDERS]}")
            self.log(f"Sharpe Ratio: {statistics[PerformanceMetrics.SHARPE_RATIO]}")

            # --------

            # We can also set custom summary statistics:

            if all(count == 0 for count in self.trade_counts.values()):
                if StatisticsResultsAlgorithm.most_traded_security_statistic in statistics:
                    raise Exception(f"Statistic {StatisticsResultsAlgorithm.most_traded_security_statistic} should not be set yet")
                if StatisticsResultsAlgorithm.most_traded_security_trade_count_statistic in statistics:
                    raise Exception(f"Statistic {StatisticsResultsAlgorithm.most_traded_security_trade_count_statistic} should not be set yet")
            else:
                # The current most traded security should be set in the summary
                most_trade_security, most_trade_security_trade_count = self.get_most_trade_security()
                self.check_most_traded_security_statistic(statistics, most_trade_security, most_trade_security_trade_count)

            # Update the trade count
            self.trade_counts[order_event.symbol] += 1

            # Set the most traded security
            most_trade_security, most_trade_security_trade_count = self.get_most_trade_security()
            self.set_summary_statistic(StatisticsResultsAlgorithm.most_traded_security_statistic, most_trade_security)
            self.set_summary_statistic(StatisticsResultsAlgorithm.most_traded_security_trade_count_statistic, most_trade_security_trade_count)

            # Re-calculate statistics:
            statistics = self.statistics.summary

            # Let's keep track of our custom summary statistics after the update
            self.check_most_traded_security_statistic(statistics, most_trade_security, most_trade_security_trade_count)

    def on_end_of_algorithm(self):
        statistics = self.statistics.summary
        if StatisticsResultsAlgorithm.most_traded_security_statistic not in statistics:
            raise Exception(f"Statistic {StatisticsResultsAlgorithm.most_traded_security_statistic} should be in the summary statistics")
        if StatisticsResultsAlgorithm.most_traded_security_trade_count_statistic not in statistics:
            raise Exception(f"Statistic {StatisticsResultsAlgorithm.most_traded_security_trade_count_statistic} should be in the summary statistics")

        most_trade_security, most_trade_security_trade_count = self.get_most_trade_security()
        self.check_most_traded_security_statistic(statistics, most_trade_security, most_trade_security_trade_count)

    def check_most_traded_security_statistic(self, statistics: Dict[str, str], most_traded_security: Symbol, trade_count: int):
        most_traded_security_statistic = statistics[StatisticsResultsAlgorithm.most_traded_security_statistic]
        most_traded_security_trade_count_statistic = statistics[StatisticsResultsAlgorithm.most_traded_security_trade_count_statistic]
        self.log(f"Most traded security: {most_traded_security_statistic}")
        self.log(f"Most traded security trade count: {most_traded_security_trade_count_statistic}")

        if most_traded_security_statistic != most_traded_security:
            raise Exception(f"Most traded security should be {most_traded_security} but it is {most_traded_security_statistic}")

        if most_traded_security_trade_count_statistic != str(trade_count):
            raise Exception(f"Most traded security trade count should be {trade_count} but it is {most_traded_security_trade_count_statistic}")

    def get_most_trade_security(self) -> Tuple[Symbol, int]:
        most_trade_security = max(self.trade_counts, key=lambda symbol: self.trade_counts[symbol])
        most_trade_security_trade_count = self.trade_counts[most_trade_security]
        return most_trade_security, most_trade_security_trade_count

