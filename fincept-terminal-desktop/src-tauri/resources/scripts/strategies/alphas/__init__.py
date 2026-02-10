# Fincept Terminal - Strategy Engine
# Auto-generated strategy package
import sys, os, types
_parent = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _parent not in sys.path:
    sys.path.insert(0, _parent)

from AlgorithmImports import (
    AlphaModel, ConstantAlphaModel, CompositeAlphaModel,
    Insight, InsightDirection, InsightType, InsightScore,
    InsightCollection
)

# Common alpha model stubs
class RsiAlphaModel(AlphaModel):
    def __init__(self, period=14, resolution=None):
        self._period = period

class HistoricalReturnsAlphaModel(AlphaModel):
    def __init__(self, lookback=1, resolution=None):
        self._lookback = lookback

class EmaCrossAlphaModel(AlphaModel):
    def __init__(self, fast_period=12, slow_period=26, resolution=None):
        self._fast = fast_period
        self._slow = slow_period

class MacdAlphaModel(AlphaModel):
    def __init__(self, fast_period=12, slow_period=26, signal_period=9, *args, **kwargs):
        pass

class PairsTradingAlphaModel(AlphaModel):
    def __init__(self, *args, **kwargs):
        pass

class BasePairsTradingAlphaModel(AlphaModel):
    def __init__(self, *args, **kwargs):
        pass

class PearsonCorrelationPairsTradingAlphaModel(AlphaModel):
    def __init__(self, *args, **kwargs):
        pass

# Create submodule stubs so `from Alphas.RsiAlphaModel import ...` works
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
