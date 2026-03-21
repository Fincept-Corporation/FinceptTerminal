# ============================================================================
# Fincept Terminal - Execution Shim
# Re-exports from fincept_engine + dynamic submodule stubs
# ============================================================================

import sys, os, types

_parent = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _parent not in sys.path:
    sys.path.insert(0, _parent)

from fincept_engine.algorithm_imports import (
    ExecutionModel, ImmediateExecutionModel,
    VolumeWeightedAveragePriceExecutionModel,
    StandardDeviationExecutionModel
)
from fincept_engine.framework.execution import SpreadExecutionModel

_submodules = {
    'ImmediateExecutionModel': {'ImmediateExecutionModel': ImmediateExecutionModel},
    'VolumeWeightedAveragePriceExecutionModel': {'VolumeWeightedAveragePriceExecutionModel': VolumeWeightedAveragePriceExecutionModel},
    'StandardDeviationExecutionModel': {'StandardDeviationExecutionModel': StandardDeviationExecutionModel},
    'SpreadExecutionModel': {'SpreadExecutionModel': SpreadExecutionModel},
}

for _name, _attrs in _submodules.items():
    _mod = types.ModuleType(f'Execution.{_name}')
    for _attr_name, _attr_val in _attrs.items():
        setattr(_mod, _attr_name, _attr_val)
    sys.modules[f'Execution.{_name}'] = _mod
