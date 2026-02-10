"""Financial Analysis Package

A comprehensive CFA-aligned financial statement analysis package providing:
- Core data processing and base analysis infrastructure
- Statement-level analyzers (Income Statement, Balance Sheet, Cash Flow)
- Specialized analysis tools (Inventory, Assets, Taxes, Quality)
- Financial modeling capabilities

Aligned with CFA Level I & II Financial Statement Analysis curriculum.
"""

# Core infrastructure
from .core.data_processor import (
    DataProcessor,
    FinancialStatements,
    CompanyInfo,
    FinancialPeriod,
    ReportingStandard,
    DataSource
)
from .core.base_analyzer import (
    BaseAnalyzer,
    AnalysisResult,
    AnalysisType,
    RiskLevel,
    TrendDirection,
    ComparativeAnalysis,
    QualityAssessment
)

# Statement analyzers
from .statement_analyzers.income_statement import IncomeStatementAnalyzer
from .statement_analyzers.balance_sheet import BalanceSheetAnalyzer
from .statement_analyzers.cash_flow import CashFlowAnalyzer
from .statement_analyzers.comprehensive_analyzer import ComprehensiveAnalyzer

# Specialized analysis
from .specialized_analysis.inventory_analysis import InventoryAnalyzer
from .specialized_analysis.asset_analysis import LongTermAssetAnalyzer
from .specialized_analysis.tax_analysis import TaxAnalyzer
from .specialized_analysis.quality_analysis import FinancialReportingQualityAnalyzer
from .specialized_analysis.financial_modeling import FinancialStatementModelingAnalyzer

__version__ = "1.1.0"

__all__ = [
    # Core - Data Processing
    "DataProcessor",
    "FinancialStatements",
    "CompanyInfo",
    "FinancialPeriod",
    "ReportingStandard",
    "DataSource",

    # Core - Base Analyzer
    "BaseAnalyzer",
    "AnalysisResult",
    "AnalysisType",
    "RiskLevel",
    "TrendDirection",
    "ComparativeAnalysis",
    "QualityAssessment",

    # Statement Analyzers
    "IncomeStatementAnalyzer",
    "BalanceSheetAnalyzer",
    "CashFlowAnalyzer",
    "ComprehensiveAnalyzer",

    # Specialized Analysis
    "InventoryAnalyzer",
    "LongTermAssetAnalyzer",
    "TaxAnalyzer",
    "FinancialReportingQualityAnalyzer",
    "FinancialStatementModelingAnalyzer",
]
