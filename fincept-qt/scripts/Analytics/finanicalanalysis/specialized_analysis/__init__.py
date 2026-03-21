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
from .financial_modeling import FinancialStatementModelingAnalyzer

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
