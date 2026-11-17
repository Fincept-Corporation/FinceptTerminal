"""M&A Deal Database Module"""
import sys
from pathlib import Path

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Resolved on first use (PEP 562): importing `corporateFinance.deal_database.database_schema` (precedents,
# regression, valuation summary) must not load the scanner and parser as well.
_LAZY = {
    'MADatabase': 'corporateFinance.deal_database.database_schema',
    'MADealScanner': 'corporateFinance.deal_database.deal_scanner',
    'MADealParser': 'corporateFinance.deal_database.deal_parser',
    'MADealTracker': 'corporateFinance.deal_database.deal_tracker',
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
