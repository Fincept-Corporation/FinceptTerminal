# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-95644B6E
# Category: Regression Test
# Description: Regression algorithm demonstrating the use of zipped custom data sourced from the object store
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

from CustomDataObjectStoreRegressionAlgorithm import *

### <summary>
### Regression algorithm demonstrating the use of zipped custom data sourced from the object store
### </summary>
class CustomDataZippedObjectStoreRegressionAlgorithm(CustomDataObjectStoreRegressionAlgorithm):

    def get_custom_data_key(self):
        return "CustomData/ExampleCustomData.zip"

    def save_data_to_object_store(self):
        self.object_store.save_bytes(self.get_custom_data_key(), Compression.zip_bytes(bytes(self.custom_data, 'utf-8'), 'data'))

