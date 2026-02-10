import sys, os, types
_parent = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _parent not in sys.path:
    sys.path.insert(0, _parent)

from AlgorithmImports import (
    UniverseSelectionModel,
    ManualUniverseSelectionModel,
    FundamentalUniverseSelectionModel,
    ScheduledUniverseSelectionModel,
    CustomUniverseSelectionModel,
    CoarseFundamental, FineFundamental,
    UniverseSettings, Universe, Field
)

class OptionUniverseSelectionModel(UniverseSelectionModel):
    def __init__(self, *args, **kwargs):
        pass

class FutureUniverseSelectionModel(UniverseSelectionModel):
    def __init__(self, *args, **kwargs):
        pass

class ETFConstituentsUniverseSelectionModel(UniverseSelectionModel):
    def __init__(self, *args, **kwargs):
        pass

class QC500UniverseSelectionModel(FundamentalUniverseSelectionModel):
    pass

_submodules = {
    'ManualUniverseSelectionModel': {'ManualUniverseSelectionModel': ManualUniverseSelectionModel},
    'FundamentalUniverseSelectionModel': {'FundamentalUniverseSelectionModel': FundamentalUniverseSelectionModel},
    'ScheduledUniverseSelectionModel': {'ScheduledUniverseSelectionModel': ScheduledUniverseSelectionModel},
    'OptionUniverseSelectionModel': {'OptionUniverseSelectionModel': OptionUniverseSelectionModel},
    'FutureUniverseSelectionModel': {'FutureUniverseSelectionModel': FutureUniverseSelectionModel},
    'ETFConstituentsUniverseSelectionModel': {'ETFConstituentsUniverseSelectionModel': ETFConstituentsUniverseSelectionModel},
    'QC500UniverseSelectionModel': {'QC500UniverseSelectionModel': QC500UniverseSelectionModel},
}

for _name, _attrs in _submodules.items():
    _mod = types.ModuleType(f'Selection.{_name}')
    for _attr_name, _attr_val in _attrs.items():
        setattr(_mod, _attr_name, _attr_val)
    sys.modules[f'Selection.{_name}'] = _mod
