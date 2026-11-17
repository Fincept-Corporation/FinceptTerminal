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
### </summary>
class CustomDataTypeHistoryAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2017, 8, 20)
        self.set_end_date(2017, 8, 20)

        self._symbol = self.add_data(CustomDataType, "CustomDataType", Resolution.HOUR).symbol

        history = list(self.history[CustomDataType](self._symbol, 48, Resolution.HOUR))

        if len(history) == 0:
            raise Exception("History request returned no data")

        self._assert_history_data(history)

        history2 = list(self.history[CustomDataType]([self._symbol], 48, Resolution.HOUR))

        if len(history2) != len(history):
            raise Exception("History requests returned different data")

        self._assert_history_data([y.values()[0] for y in history2])

    def _assert_history_data(self, history:  List[PythonData]) -> None:
        expected_keys = ['open', 'close', 'high', 'low', 'some_property']
        if any(any(not x[key] for key in expected_keys)
               or x["some_property"] != "some property value"
               for x in history):
            raise Exception("History request returned data without the expected properties")

class CustomDataType(PythonData):

    def get_source(self, config: SubscriptionDataConfig, date: datetime, is_live: bool) -> SubscriptionDataSource:
        source = "https://www.dl.dropboxusercontent.com/s/d83xvd7mm9fzpk0/path_to_my_csv_data.csv?dl=0"
        return SubscriptionDataSource(source, SubscriptionTransportMedium.REMOTE_FILE)

    def reader(self, config: SubscriptionDataConfig, line: str, date: datetime, is_live: bool) -> BaseData:
        if not (line.strip()):
            return None

        data = line.split(',')
        obj_data = CustomDataType()
        obj_data.symbol = config.symbol

        try:
            obj_data.time = datetime.strptime(data[0], '%Y-%m-%d %H:%M:%S') + timedelta(hours=20)
            obj_data["open"] = float(data[1])
            obj_data["high"] = float(data[2])
            obj_data["low"] = float(data[3])
            obj_data["close"] = float(data[4])
            obj_data.value = obj_data["close"]

            # property for asserting the correct data is fetched
            obj_data["some_property"] = "some property value"
        except ValueError:
            return None

        return obj_data
