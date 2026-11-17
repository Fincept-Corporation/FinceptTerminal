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

from CustomDataObjectStoreRegressionAlgorithm import *

### <summary>
### Regression algorithm demonstrating the use of zipped custom data sourced from the object store
### </summary>
class CustomDataZippedObjectStoreRegressionAlgorithm(CustomDataObjectStoreRegressionAlgorithm):

    def get_custom_data_key(self):
        return "CustomData/ExampleCustomData.zip"

    def save_data_to_object_store(self):
        self.object_store.save_bytes(self.get_custom_data_key(), Compression.zip_bytes(bytes(self.custom_data, 'utf-8'), 'data'))

