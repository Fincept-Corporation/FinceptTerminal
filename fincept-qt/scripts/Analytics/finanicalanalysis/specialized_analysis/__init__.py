"""Specialized Analysis Module

This module contains specialized financial analysis tools aligned with CFA curriculum:
- Inventory Analysis (FIFO, LIFO, inventory valuation)
- Asset Analysis (Long-term assets, depreciation, impairment)
- Tax Analysis (Deferred taxes, ETR analysis, tax provisions)
- Quality Analysis (Financial reporting quality, earnings quality, manipulation detection)
- Financial Modeling (Pro forma statements, forecasting, sensitivity analysis)
"""

from .inventory_analysis import InventoryAnalyzer
from .asset_analysis import LongTermAssetAnalyzer
from .tax_analysis import TaxAnalyzer
from .quality_analysis import FinancialReportingQualityAnalyzer

__all__ = [
    # Inventory Analysis
    "InventoryAnalyzer",

    # Long-Term Asset Analysis
    "LongTermAssetAnalyzer",

    # Tax Analysis
    "TaxAnalyzer",

    # Financial Reporting Quality Analysis
    "FinancialReportingQualityAnalyzer",

    # Financial Statement Modeling
    "FinancialStatementModelingAnalyzer",
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "FinancialStatementModelingAnalyzer": ("financial_modeling", "FinancialStatementModelingAnalyzer"),
}


def __getattr__(name: str):  # PEP 562
    target = _LAZY_ATTRS.get(name)
    if target is None:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    submodule, original_name = target
    import importlib
    mod = importlib.import_module(f".{submodule}", __name__)
    value = getattr(mod, original_name)
    globals()[name] = value  # cache for subsequent access
    return value


def __dir__() -> list[str]:
    return sorted(set(globals()) | set(_LAZY_ATTRS))
