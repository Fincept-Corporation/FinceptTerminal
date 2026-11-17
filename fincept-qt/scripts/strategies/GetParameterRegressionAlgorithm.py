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
### Regression algorithm for testing parameterized regression algorithms get valid parameters.
### </summary>
class GetParameterRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2013, 10, 7)
        self.check_parameter(None, self.get_parameter("non-existing"), "GetParameter(\"non-existing\")")
        self.check_parameter("100", self.get_parameter("non-existing", "100"), "GetParameter(\"non-existing\", \"100\")")
        self.check_parameter(100, self.get_parameter("non-existing", 100), "GetParameter(\"non-existing\", 100)")
        self.check_parameter(100.0, self.get_parameter("non-existing", 100.0), "GetParameter(\"non-existing\", 100.0)")

        self.check_parameter("10", self.get_parameter("ema-fast"), "GetParameter(\"ema-fast\")")
        self.check_parameter(10, self.get_parameter("ema-fast", 100), "GetParameter(\"ema-fast\", 100)")
        self.check_parameter(10.0, self.get_parameter("ema-fast", 100.0), "GetParameter(\"ema-fast\", 100.0)")

        self.quit()

    def check_parameter(self, expected, actual, call):
        if expected == None and actual != None:
            raise Exception(f"{call} should have returned null but returned {actual} ({type(actual)})")

        if expected != None and actual == None:
            raise Exception(f"{call} should have returned {expected} ({type(expected)}) but returned null")

        if expected != None and actual != None and type(expected) != type(actual) or expected != actual:
            raise Exception(f"{call} should have returned {expected} ({type(expected)}) but returned {actual} ({type(actual)})")
