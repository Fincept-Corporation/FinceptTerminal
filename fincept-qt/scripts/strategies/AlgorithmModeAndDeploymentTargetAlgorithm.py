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
### Algorithm asserting the correct values for the deployment target and algorithm mode.
### </summary>
class AlgorithmModeAndDeploymentTargetAlgorithm(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2013,10, 7)
        self.set_end_date(2013,10,11)
        self.set_cash(100000)

        #translate commented code from c# to python
        self.debug(f"Algorithm Mode: {self.algorithm_mode}. Is Live Mode: {self.live_mode}. Deployment Target: {self.deployment_target}.")

        if self.algorithm_mode != AlgorithmMode.BACKTESTING:
            raise Exception(f"Algorithm mode is not backtesting. Actual: {self.algorithm_mode}")

        if self.live_mode:
            raise Exception("Algorithm should not be live")

        if self.deployment_target != DeploymentTarget.LOCAL_PLATFORM:
            raise Exception(f"Algorithm deployment target is not local. Actual{self.deployment_target}")

        # For a live deployment these checks should pass:
        # if self.algorithm_mode != AlgorithmMode.LIVE: raise Exception("Algorithm mode is not live")
        # if not self.live_mode: raise Exception("Algorithm should be live")

        # For a cloud deployment these checks should pass:
        # if self.deployment_target != DeploymentTarget.CLOUD_PLATFORM: raise Exception("Algorithm deployment target is not cloud")

        self.quit()
