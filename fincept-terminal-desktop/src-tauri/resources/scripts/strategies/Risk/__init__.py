import sys, os, types
_parent = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _parent not in sys.path:
    sys.path.insert(0, _parent)

from AlgorithmImports import (
    RiskManagementModel, NullRiskManagementModel,
    MaximumDrawdownPercentPortfolio,
    MaximumDrawdownPercentPerSecurity,
    MaximumUnrealizedProfitPercentPerSecurity,
    TrailingStopRiskManagementModel,
    MaximumSectorExposureRiskManagementModel,
    CompositeRiskManagementModel
)

_submodules = {
    'MaximumDrawdownPercentPortfolio': {'MaximumDrawdownPercentPortfolio': MaximumDrawdownPercentPortfolio},
    'MaximumDrawdownPercentPerSecurity': {'MaximumDrawdownPercentPerSecurity': MaximumDrawdownPercentPerSecurity},
    'MaximumUnrealizedProfitPercentPerSecurity': {'MaximumUnrealizedProfitPercentPerSecurity': MaximumUnrealizedProfitPercentPerSecurity},
    'TrailingStopRiskManagementModel': {'TrailingStopRiskManagementModel': TrailingStopRiskManagementModel},
    'MaximumSectorExposureRiskManagementModel': {'MaximumSectorExposureRiskManagementModel': MaximumSectorExposureRiskManagementModel},
    'CompositeRiskManagementModel': {'CompositeRiskManagementModel': CompositeRiskManagementModel},
    'NullRiskManagementModel': {'NullRiskManagementModel': NullRiskManagementModel},
}

for _name, _attrs in _submodules.items():
    _mod = types.ModuleType(f'Risk.{_name}')
    for _attr_name, _attr_val in _attrs.items():
        setattr(_mod, _attr_name, _attr_val)
    sys.modules[f'Risk.{_name}'] = _mod
