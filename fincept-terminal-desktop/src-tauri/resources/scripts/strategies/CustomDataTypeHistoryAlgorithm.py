# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-1A4DBA47
# Category: Custom Data
# Description: Custom Data Type History Algorithm
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
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
