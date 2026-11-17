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
from QuantConnect.Algorithm.CSharp import *

### <summary>
### Regression algorithm asserting we can specify a custom option assignment
### </summary>
class CustomOptionAssignmentRegressionAlgorithm(OptionAssignmentRegressionAlgorithm):
    def initialize(self):
        self.set_security_initializer(self.custom_security_initializer)
        super().initialize()

    def custom_security_initializer(self, security):
        if Extensions.is_option(security.symbol.security_type):
            # we have to be 10% in the money to get assigned
            security.set_option_assignment_model(PyCustomOptionAssignmentModel(0.1))

    def on_data(self, data):
        super().on_data(data)

class PyCustomOptionAssignmentModel(DefaultOptionAssignmentModel):
    def __init__(self, required_in_the_money_percent):
        super().__init__(required_in_the_money_percent)

    def get_assignment(self, parameters):
        result = super().get_assignment(parameters)
        result.tag = "Custom Option Assignment"
        return result
