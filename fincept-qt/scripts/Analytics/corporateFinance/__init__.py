"""Corporate Finance & M&A Analysis Module"""
import sys
from pathlib import Path

__version__ = "1.0.0"

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent
sys.path.insert(0, str(analytics_path))

# The deal-database classes are resolved on first use (PEP 562), not on package import. Importing them eagerly here
# made every script that does `from corporateFinance.<anything> import ...` (LBO, merger model, contribution,
# trading comps, precedents, startup summary) pay for the deal tracker and its dependencies - about 5 s per run.
_LAZY = {
    'MADealTracker': 'corporateFinance.deal_database.deal_tracker',
    'MADealScanner': 'corporateFinance.deal_database.deal_scanner',
    'MADealParser': 'corporateFinance.deal_database.deal_parser',
}
__all__ = list(_LAZY)


def __getattr__(name):
    module = _LAZY.get(name)
    if module is None:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    import importlib
    value = getattr(importlib.import_module(module), name)
    globals()[name] = value
    return value
