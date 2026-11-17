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
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

from AlgorithmImports import *

### <summary>
### Regression algorithm to demonstrate the use of SetBenchmark() with custom data
### </summary>
class CustomDataBenchmarkRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2017, 8, 18)  # Set Start Date
        self.set_end_date(2017, 8, 21)  # Set End Date
        self.set_cash(100000)  # Set Strategy Cash

        self.add_equity("SPY", Resolution.HOUR)
        # Load benchmark data
        self.custom_symbol = self.add_data(ExampleCustomData, "ExampleCustomData", Resolution.HOUR).symbol
        self.set_benchmark(self.custom_symbol)

    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings("SPY", 1)

    def on_end_of_algorithm(self):
        security_benchmark = self.benchmark
        if security_benchmark.security.price == 0:
            raise Exception("Security benchmark price was not expected to be zero")

class ExampleCustomData(PythonData):

    def get_source(self, config, date, is_live):
        source = "https://www.dl.dropboxusercontent.com/s/d83xvd7mm9fzpk0/path_to_my_csv_data.csv?dl=0"
        return SubscriptionDataSource(source, SubscriptionTransportMedium.REMOTE_FILE)

    def reader(self, config, line, date, is_live):
        data = line.split(',')
        obj_data = ExampleCustomData()
        obj_data.symbol = config.symbol
        obj_data.time = datetime.strptime(data[0], '%Y-%m-%d %H:%M:%S') + timedelta(hours=20)
        obj_data.value = float(data[4])
        obj_data["Open"] = float(data[1])
        obj_data["High"] = float(data[2])
        obj_data["Low"] = float(data[3])
        obj_data["Close"] = float(data[4])
        return obj_data
