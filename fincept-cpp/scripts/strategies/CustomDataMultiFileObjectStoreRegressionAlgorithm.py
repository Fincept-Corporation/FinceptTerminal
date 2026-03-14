# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-EA427FD2
# Category: Regression Test
# Description: Regression algorithm demonstrating the use of custom data sourced from multiple "files" in the object store
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from datetime import datetime

### <summary>
### Regression algorithm demonstrating the use of custom data sourced from multiple "files" in the object store
### </summary>
class CustomDataMultiFileObjectStoreRegressionAlgorithm(QCAlgorithm):
    custom_data = "2017-08-18 01:00:00,5749.5,5852.95,5749.5,5842.2,214402430,8753.33\n2017-08-18 02:00:00,5834.1,5904.35,5822.2,5898.85,144794030,5405.72\n2017-08-18 03:00:00,5885.5,5898.8,5852.3,5857.55,145721790,5163.09\n2017-08-18 04:00:00,5811.95,5815,5760.4,5770.9,160523863,5219.24\n2017-08-18 05:00:00,5794.75,5848.2,5786.05,5836.95,151929179,5429.87\n2017-08-18 06:00:00,5889.95,5900.45,5858.45,5867.9,123586417,4303.93\n2017-08-18 07:00:00,5833.15,5833.85,5775.55,5811.55,127624733,4823.52\n2017-08-18 08:00:00,5834.6,5864.95,5834.6,5859,110427867,4661.55\n2017-08-18 09:00:00,5869.9,5879.35,5802.85,5816.7,117516350,4820.53\n2017-08-18 10:00:00,5894.5,5948.85,5887.95,5935.1,120195681,4882.29\n2017-08-18 11:00:00,6000.5,6019,5951.15,6009,127707078,6591.27\n2017-08-18 12:00:00,5991.2,6038.2,5980.95,6030.8,116275729,4641.97\n2017-08-18 13:00:00,5930.8,5966.05,5910.95,5955.25,151162819,5915.8\n2017-08-18 14:00:00,5972.25,5989.8,5926.75,5973.3,191516153,8349.59\n2017-08-18 15:00:00,5984.7,6051.1,5974.55,6038.05,171728134,7774.83\n2017-08-18 16:00:00,5894.5,5948.85,5887.95,5935.1,120195681,4882.29\n2017-08-18 17:00:00,6000.5,6019,5951.15,6009,127707078,6591.27\n2017-08-18 18:00:00,5991.2,6038.2,5980.95,6030.8,116275729,4641.97\n2017-08-18 19:00:00,5894.5,5948.85,5887.95,5935.1,120195681,4882.29\n2017-08-18 20:00:00,5895,5956.55,5869.5,5921.4,114174694,4961.54\n2017-08-18 21:00:00,5900.05,5972.7,5871.3,5881,118346364,4888.65\n2017-08-18 22:00:00,5907.9,5931.65,5857.4,5878,100130739,4304.75\n2017-08-18 23:00:00,5848.75,5868.05,5780.35,5788.8,180902123,6695.57\n2017-08-19 01:00:00,5771.75,5792.9,5738.6,5760.2,140394424,5894.04\n2017-08-19 02:00:00,5709.35,5729.85,5683.1,5699.1,142041404,5462.45\n2017-08-19 03:00:00,5748.95,5819.4,5739.4,5808.4,124410018,5121.33\n2017-08-19 04:00:00,5820.4,5854.9,5770.25,5850.05,107160887,4560.84\n2017-08-19 05:00:00,5841.9,5863.4,5804.3,5813.6,117541145,4591.91\n2017-08-19 06:00:00,5805.75,5828.4,5777.9,5822.25,115539008,4643.17\n2017-08-19 07:00:00,5754.15,5755,5645.65,5655.9,198400131,7148\n2017-08-19 08:00:00,5639.9,5686.15,5616.85,5667.65,182410583,6697.18\n2017-08-19 09:00:00,5638.05,5640,5566.25,5590.25,193488581,6308.88\n2017-08-19 10:00:00,5606.95,5666.25,5570.25,5609.1,196571543,6792.49\n2017-08-19 11:00:00,5627.95,5635.25,5579.35,5588.7,160095940,5939.3\n2017-08-19 12:00:00,5647.95,5699.35,5630.95,5682.35,239029425,9184.29\n2017-08-19 13:00:00,5749.5,5852.95,5749.5,5842.2,214402430,8753.33\n2017-08-19 14:00:00,5834.1,5904.35,5822.2,5898.85,144794030,5405.72\n2017-08-19 15:00:00,5885.5,5898.8,5852.3,5857.55,145721790,5163.09\n2017-08-19 16:00:00,5811.95,5815,5760.4,5770.9,160523863,5219.24\n2017-08-19 17:00:00,5794.75,5848.2,5786.05,5836.95,151929179,5429.87\n2017-08-19 18:00:00,5889.95,5900.45,5858.45,5867.9,123586417,4303.93\n2017-08-19 19:00:00,5833.15,5833.85,5775.55,5811.55,127624733,4823.52\n2017-08-19 20:00:00,5834.6,5864.95,5834.6,5859,110427867,4661.55\n2017-08-19 21:00:00,5869.9,5879.35,5802.85,5816.7,117516350,4820.53\n2017-08-19 22:00:00,5894.5,5948.85,5887.95,5935.1,120195681,4882.29\n2017-08-19 23:00:00,6000.5,6019,5951.15,6009,127707078,6591.27\n2017-08-21 01:00:00,5749.5,5852.95,5749.5,5842.2,214402430,8753.33\n2017-08-21 02:00:00,5834.1,5904.35,5822.2,5898.85,144794030,5405.72\n2017-08-21 03:00:00,5885.5,5898.8,5852.3,5857.55,145721790,5163.09\n2017-08-21 04:00:00,5811.95,5815,5760.4,5770.9,160523863,5219.24\n2017-08-21 05:00:00,5794.75,5848.2,5786.05,5836.95,151929179,5429.87\n2017-08-21 06:00:00,5889.95,5900.45,5858.45,5867.9,123586417,4303.93\n2017-08-21 07:00:00,5833.15,5833.85,5775.55,5811.55,127624733,4823.52\n2017-08-21 08:00:00,5834.6,5864.95,5834.6,5859,110427867,4661.55\n2017-08-21 09:00:00,5869.9,5879.35,5802.85,5816.7,117516350,4820.53\n2017-08-21 10:00:00,5894.5,5948.85,5887.95,5935.1,120195681,4882.29\n2017-08-21 11:00:00,6000.5,6019,5951.15,6009,127707078,6591.27\n2017-08-21 12:00:00,5991.2,6038.2,5980.95,6030.8,116275729,4641.97\n2017-08-21 13:00:00,5930.8,5966.05,5910.95,5955.25,151162819,5915.8\n2017-08-21 14:00:00,5972.25,5989.8,5926.75,5973.3,191516153,8349.59\n2017-08-21 15:00:00,5984.7,6051.1,5974.55,6038.05,171728134,7774.83\n2017-08-21 16:00:00,5894.5,5948.85,5887.95,5935.1,120195681,4882.29\n2017-08-21 17:00:00,6000.5,6019,5951.15,6009,127707078,6591.27\n2017-08-21 18:00:00,5991.2,6038.2,5980.95,6030.8,116275729,4641.97\n2017-08-21 19:00:00,5894.5,5948.85,5887.95,5935.1,120195681,4882.29\n2017-08-21 20:00:00,5895,5956.55,5869.5,5921.4,114174694,4961.54\n2017-08-21 21:00:00,5900.05,5972.7,5871.3,5881,118346364,4888.65\n2017-08-21 22:00:00,5907.9,5931.65,5857.4,5878,100130739,4304.75\n2017-08-21 23:00:00,5848.75,5868.05,5780.35,5788.8,180902123,6695.57"

    def initialize(self):
        self.set_start_date(2017, 8, 18)
        self.set_end_date(2017, 8, 21)
        self.set_cash(100000)

        self.set_benchmark(lambda x: 0)

        self.custom_symbol = self.add_data(ExampleCustomData, "ExampleCustomData", Resolution.HOUR).symbol

        # Saving data here for demonstration and regression testing purposes.
        # In real scenarios, data has to be saved to the object store before the algorithm starts.
        # Note: The data is stored in separate files for each date.
        data = {}
        for line in self.custom_data.split('\n'):
            csv = line.split(',')
            time = datetime.strptime(csv[0], '%Y-%m-%d %H:%M:%S').date()
            if time not in data:
                data[time] = line
            else:
                data[time] += '\n' + line

        for date, date_data in data.items():
            self.object_store.save(self.get_custom_data_key(date), date_data)

        self.received_data = []

    def on_data(self, slice: Slice):
        if slice.contains_key(self.custom_symbol):
            custom_data = slice.get(ExampleCustomData, self.custom_symbol)
            if custom_data.price == 0:
                raise Exception("Custom data price was not expected to be zero")

            self.received_data.append(custom_data)

    def on_end_of_algorithm(self):
        if not self.received_data:
            raise Exception("Custom data was not fetched")

        custom_security = self.securities[self.custom_symbol]
        if custom_security is None or custom_security.price == 0:
            Exception("Expected the custom security to be added to the algorithm securities and to have a price that is not zero")

        # Make sure history requests work as expected
        history = self.history(ExampleCustomData, self.custom_symbol, self.start_date, self.end_date, Resolution.HOUR)

        if history.shape[0] != len(self.received_data):
            raise Exception("History request returned more or less data than expected")

        for i in range(len(self.received_data)):
            received_data = self.received_data[i]
            if (history.index[i][0] != received_data.symbol or
                history.index[i][1] != received_data.time or
                history[["value"]].values[i][0] != received_data.value or
                history[["open"]].values[i][0] != received_data.open or
                history[["high"]].values[i][0] != received_data.high or
                history[["low"]].values[i][0] != received_data.low or
                history[["close"]].values[i][0] != received_data.close):
                raise Exception("History request returned different data than expected")

    @staticmethod
    def get_custom_data_key(date: datetime.date) -> str:
        return f"CustomData/ExampleCustomData{date.strftime('%Y%m%d')}"

class ExampleCustomData(PythonData):
    CustomDataKey = ""

    def get_source(self, config, date, is_live):
        return SubscriptionDataSource(CustomDataMultiFileObjectStoreRegressionAlgorithm.get_custom_data_key(date.date()),
                                      SubscriptionTransportMedium.OBJECT_STORE,
                                      FileFormat.CSV)

    def reader(self, config, line, date, is_live):
        data = line.split(',')
        obj_data = ExampleCustomData()
        obj_data.symbol = config.symbol
        obj_data.time = datetime.strptime(data[0], '%Y-%m-%d %H:%M:%S')
        obj_data.value = float(data[4])
        obj_data["Open"] = float(data[1])
        obj_data["High"] = float(data[2])
        obj_data["Low"] = float(data[3])
        obj_data["Close"] = float(data[4])
        return obj_data
