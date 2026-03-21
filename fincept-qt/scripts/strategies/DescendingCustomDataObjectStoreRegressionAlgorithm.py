# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-6513B96C
# Category: Regression Test
# Description: The regression algorithm showcases the utilization of a custom data source with the Sort flag set to true. This means...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *


### <summary>
### The regression algorithm showcases the utilization of a custom data source with the Sort flag set to true.
### This means that the source initially provides data in descending order, which is then organized into ascending order and returned in the 'on_data' function.
### </summary>
class DescendingCustomDataObjectStoreRegressionAlgorithm(QCAlgorithm):
    descending_custom_data = [
        "2024-10-03 19:00:00,173.5,176.0,172.0,175.2,120195681,4882.29",
        "2024-10-02 18:00:00,174.0,177.0,173.0,175.8,116275729,4641.97",
        "2024-10-01 17:00:00,175.0,178.0,172.5,174.5,127707078,6591.27",
        "2024-09-30 11:00:00,174.8,176.5,172.8,175.0,127707078,6591.27",
        "2024-09-27 10:00:00,172.5,175.0,171.5,173.5,120195681,4882.29",
        "2024-09-26 09:00:00,171.0,172.5,170.0,171.8,117516350,4820.53",
        "2024-09-25 08:00:00,169.5,172.0,169.0,171.0,110427867,4661.55",
        "2024-09-24 07:00:00,170.0,171.0,168.0,169.5,127624733,4823.52",
        "2024-09-23 06:00:00,172.0,173.5,169.5,171.5,123586417,4303.93",
        "2024-09-20 05:00:00,168.0,171.0,167.5,170.5,151929179,5429.87",
        "2024-09-19 04:00:00,170.5,171.5,166.0,167.0,160523863,5219.24",
        "2024-09-18 03:00:00,173.0,174.0,169.0,172.0,145721790,5163.09",
        "2024-09-17 02:00:00,171.0,173.5,170.0,172.5,144794030,5405.72",
        "2024-09-16 01:00:00,168.0,171.0,167.0,170.0,214402430,8753.33",
        "2024-09-13 16:00:00,173.5,176.0,172.0,175.2,120195681,4882.29",
        "2024-09-12 15:00:00,174.5,177.5,173.5,176.5,171728134,7774.83",
        "2024-09-11 14:00:00,175.0,178.0,174.0,175.5,191516153,8349.59",
        "2024-09-10 13:00:00,174.5,176.0,173.0,174.0,151162819,5915.8",
        "2024-09-09 12:00:00,176.0,178.0,175.0,177.0,116275729,4641.97"
    ]

    def initialize(self):
        self.set_start_date(2024, 9, 9)
        self.set_end_date(2024, 10, 3)
        self.set_cash(100000)

        self.set_benchmark(lambda x: 0)

        SortCustomData.custom_data_key = self.get_custom_data_key()

        self.custom_symbol = self.add_data(SortCustomData, "SortCustomData", Resolution.DAILY).symbol

        # Saving data here for demonstration and regression testing purposes.
        # In real scenarios, data has to be saved to the object store before the algorithm starts.
        self.object_store.save(self.get_custom_data_key(), "\n".join(self.descending_custom_data))

        self.received_data = []

    def on_data(self, slice: Slice):
        if slice.contains_key(self.custom_symbol):
            custom_data = slice.get(SortCustomData, self.custom_symbol)
            if custom_data.open == 0 or custom_data.high == 0 or custom_data.low == 0 or custom_data.close == 0 or custom_data.price == 0:
                raise Exception("One or more custom data fields (open, high, low, close, price) are zero.")

            self.received_data.append(custom_data)

    def on_end_of_algorithm(self):
        if not self.received_data:
            raise Exception("Custom data was not fetched")

        # Make sure history requests work as expected
        history = self.history(SortCustomData, self.custom_symbol, self.start_date, self.end_date, Resolution.DAILY)

        if history.shape[0] != len(self.received_data):
            raise Exception("History request returned more or less data than expected")

        # Iterate through the history collection, checking if the time is in ascending order.
        for i in range(len(history) - 1):
            # [1] - time
            if history.index[i][1] > history.index[i + 1][1]:
                raise RegressionTestException(
                    f"Order failure: {history.index[i][1]} > {history.index[i + 1][1]} at index {i}.")

    def get_custom_data_key(self):
        return "CustomData/SortCustomData"


class SortCustomData(PythonData):
    custom_data_key = ""

    def get_source(self, config, date, is_live):
        subscription = SubscriptionDataSource(self.custom_data_key, SubscriptionTransportMedium.OBJECT_STORE,
                                              FileFormat.CSV)
        # Indicate that the data from the subscription will be returned in descending order.
        subscription.Sort = True
        return subscription

    def reader(self, config, line, date, is_live):
        data = line.split(',')
        obj_data = SortCustomData()
        obj_data.symbol = config.symbol
        obj_data.time = datetime.strptime(data[0], '%Y-%m-%d %H:%M:%S')
        obj_data.value = float(data[4])
        obj_data["Open"] = float(data[1])
        obj_data["High"] = float(data[2])
        obj_data["Low"] = float(data[3])
        obj_data["Close"] = float(data[4])
        return obj_data
