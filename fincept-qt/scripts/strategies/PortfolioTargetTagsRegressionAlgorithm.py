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
### Algorithm demonstrating the portfolio target tags usage
### </summary>
class PortfolioTargetTagsRegressionAlgorithm(QCAlgorithm):
    '''Algorithm demonstrating the portfolio target tags usage'''

    def initialize(self):
        self.set_start_date(2013,10,7)   #Set Start Date
        self.set_end_date(2013,10,11)    #Set End Date
        self.set_cash(100000)           #Set Strategy Cash

        self.universe_settings.resolution = Resolution.MINUTE

        symbols = [ Symbol.create("SPY", SecurityType.EQUITY, Market.USA) ]

        # set algorithm framework models
        self.set_universe_selection(ManualUniverseSelectionModel(symbols))
        self.set_alpha(ConstantAlphaModel(InsightType.PRICE, InsightDirection.UP, timedelta(minutes = 20), 0.025, None))

        self.set_portfolio_construction(CustomPortfolioConstructionModel())
        self.set_risk_management(CustomRiskManagementModel())
        self.set_execution(CustomExecutionModel(self.set_target_tags_checked))

        self.target_tags_checked = False

    def set_target_tags_checked(self):
        self.target_tags_checked = True

    def on_end_of_algorithm(self):
        if not self.target_tags_checked:
            raise Exception("The portfolio targets tag were not checked")

class CustomPortfolioConstructionModel(EqualWeightingPortfolioConstructionModel):
    def __init__(self):
        super().__init__(Resolution.DAILY)

    def create_targets(self, algorithm: QCAlgorithm, insights: List[Insight]) -> List[IPortfolioTarget]:
        targets = super().create_targets(algorithm, insights)
        return CustomPortfolioConstructionModel.add_p_portfolio_targets_tags(targets)

    @staticmethod
    def generate_portfolio_target_tag(target: IPortfolioTarget) -> str:
        return f"Portfolio target tag: {target.symbol} - {target.quantity}"

    @staticmethod
    def add_p_portfolio_targets_tags(targets: List[IPortfolioTarget]) -> List[IPortfolioTarget]:
        return [PortfolioTarget(target.symbol, target.quantity, CustomPortfolioConstructionModel.generate_portfolio_target_tag(target))
                for target in targets]

class CustomRiskManagementModel(MaximumDrawdownPercentPerSecurity):
    def __init__(self):
        super().__init__(0.01)

    def manage_risk(self, algorithm: QCAlgorithm, targets: List[IPortfolioTarget]) -> List[IPortfolioTarget]:
        risk_managed_targets = super().manage_risk(algorithm, targets)
        return CustomPortfolioConstructionModel.add_p_portfolio_targets_tags(risk_managed_targets)

class CustomExecutionModel(ImmediateExecutionModel):
    def __init__(self, targets_tag_checked_callback: Callable) -> None:
        super().__init__()
        self.targets_tag_checked_callback = targets_tag_checked_callback

    def execute(self, algorithm: QCAlgorithm, targets: List[IPortfolioTarget]) -> None:
        if len(targets) > 0:
            self.targets_tag_checked_callback()

        for target in targets:
            expected_tag = CustomPortfolioConstructionModel.generate_portfolio_target_tag(target)
            if target.tag != expected_tag:
                raise Exception(f"Unexpected portfolio target tag: {target.tag} - Expected: {expected_tag}")

        super().execute(algorithm, targets)
