# ============================================================================
# Fincept Terminal - Alphas Shim
# Re-exports from fincept_engine.framework.alphas + dynamic submodule stubs
# ============================================================================

import sys, os, types

_parent = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _parent not in sys.path:
    sys.path.insert(0, _parent)

from fincept_engine.algorithm_imports import (
    AlphaModel, ConstantAlphaModel, CompositeAlphaModel,
    Insight, InsightDirection, InsightType, InsightScore,
    InsightCollection
)
from fincept_engine.framework.alphas import (
    RsiAlphaModel, HistoricalReturnsAlphaModel, EmaCrossAlphaModel,
    MacdAlphaModel, PairsTradingAlphaModel, BasePairsTradingAlphaModel,
    PearsonCorrelationPairsTradingAlphaModel
)

# Dynamic submodule stubs: enables `from Alphas.RsiAlphaModel import RsiAlphaModel`
_submodules = {
    'RsiAlphaModel': {'RsiAlphaModel': RsiAlphaModel},
    'HistoricalReturnsAlphaModel': {'HistoricalReturnsAlphaModel': HistoricalReturnsAlphaModel},
    'EmaCrossAlphaModel': {'EmaCrossAlphaModel': EmaCrossAlphaModel},
    'MacdAlphaModel': {'MacdAlphaModel': MacdAlphaModel},
    'ConstantAlphaModel': {'ConstantAlphaModel': ConstantAlphaModel},
    'PairsTradingAlphaModel': {'PairsTradingAlphaModel': PairsTradingAlphaModel},
    'BasePairsTradingAlphaModel': {'BasePairsTradingAlphaModel': BasePairsTradingAlphaModel},
    'CompositeAlphaModel': {'CompositeAlphaModel': CompositeAlphaModel},
    'PearsonCorrelationPairsTradingAlphaModel': {'PearsonCorrelationPairsTradingAlphaModel': PearsonCorrelationPairsTradingAlphaModel},
}

for _name, _attrs in _submodules.items():
    _mod = types.ModuleType(f'Alphas.{_name}')
    for _attr_name, _attr_val in _attrs.items():
        setattr(_mod, _attr_name, _attr_val)
    sys.modules[f'Alphas.{_name}'] = _mod
