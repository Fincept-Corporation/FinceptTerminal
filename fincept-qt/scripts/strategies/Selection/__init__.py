# ============================================================================
# Fincept Terminal - Selection Shim
# Re-exports from fincept_engine + dynamic submodule stubs
# ============================================================================

import sys, os, types

_parent = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _parent not in sys.path:
    sys.path.insert(0, _parent)

from fincept_engine.algorithm_imports import (
    UniverseSelectionModel,
    ManualUniverseSelectionModel,
    FundamentalUniverseSelectionModel,
    ScheduledUniverseSelectionModel,
    CustomUniverseSelectionModel,
    CoarseFundamental, FineFundamental,
    UniverseSettings, Universe, Field,
    CoarseFundamentalUniverseSelectionModel,
    FineFundamentalUniverseSelectionModel,
    ConstituentsUniverse,
    ScheduledUniverse,
    OptionChainedUniverseSelectionModel,
    OpenInterestFutureUniverseSelectionModel
)
from fincept_engine.framework.selection import (
    OptionUniverseSelectionModel, FutureUniverseSelectionModel,
    ETFConstituentsUniverseSelectionModel, QC500UniverseSelectionModel
)

_submodules = {
    'ManualUniverseSelectionModel': {'ManualUniverseSelectionModel': ManualUniverseSelectionModel},
    'FundamentalUniverseSelectionModel': {'FundamentalUniverseSelectionModel': FundamentalUniverseSelectionModel},
    'ScheduledUniverseSelectionModel': {'ScheduledUniverseSelectionModel': ScheduledUniverseSelectionModel},
    'OptionUniverseSelectionModel': {'OptionUniverseSelectionModel': OptionUniverseSelectionModel},
    'FutureUniverseSelectionModel': {'FutureUniverseSelectionModel': FutureUniverseSelectionModel},
    'ETFConstituentsUniverseSelectionModel': {'ETFConstituentsUniverseSelectionModel': ETFConstituentsUniverseSelectionModel},
    'QC500UniverseSelectionModel': {'QC500UniverseSelectionModel': QC500UniverseSelectionModel},
    'CoarseFundamentalUniverseSelectionModel': {'CoarseFundamentalUniverseSelectionModel': CoarseFundamentalUniverseSelectionModel},
    'FineFundamentalUniverseSelectionModel': {'FineFundamentalUniverseSelectionModel': FineFundamentalUniverseSelectionModel},
    'OptionChainedUniverseSelectionModel': {'OptionChainedUniverseSelectionModel': OptionChainedUniverseSelectionModel},
    'OpenInterestFutureUniverseSelectionModel': {'OpenInterestFutureUniverseSelectionModel': OpenInterestFutureUniverseSelectionModel},
}

for _name, _attrs in _submodules.items():
    _mod = types.ModuleType(f'Selection.{_name}')
    for _attr_name, _attr_val in _attrs.items():
        setattr(_mod, _attr_name, _attr_val)
    sys.modules[f'Selection.{_name}'] = _mod
