"""
Financial Statement Base Analyzer Module
=========================================

Abstract base class providing foundation for all financial statement analyzers with
CFA-compliant methodologies. Ensures consistent analysis framework across all
specialized financial analysis modules with standardized interfaces and validation.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements (10-K, 10-Q filings)
  - Management discussion and analysis (MD&A) sections
  - Auditor reports and financial statement footnotes
  - Industry benchmarks and comparative company data
  - Economic indicators affecting financial performance

OUTPUT:
  - Standardized financial analysis framework and interfaces
  - Abstract base classes for specialized analyzers
  - Common validation rules and quality checks
  - Consistent reporting templates and formats
  - Integration points for various analysis modules

PARAMETERS:
  - analysis_period: Financial analysis period (default: 3 years)
  - industry_benchmark: Industry for comparative analysis (default: 'auto')
  - quality_threshold: Minimum financial quality score (default: 0.7)
  - currency_reporting: Reporting currency for analysis (default: 'USD')
  - compliance_standard: Compliance standard (default: 'CFA')
"""

from abc import ABC, abstractmethod
from typing import Dict, List, Optional, Union, Any, Tuple
from dataclasses import dataclass, field
from enum import Enum
import pandas as pd
import numpy as np
from datetime import datetime, date
import logging

# Import from data_processor
from .data_processor import FinancialStatements, CompanyInfo, FinancialPeriod, ReportingStandard


class AnalysisType(Enum):
    """Types of financial analysis"""
    LIQUIDITY = "liquidity"
    ACTIVITY = "activity"
    SOLVENCY = "solvency"
    PROFITABILITY = "profitability"
    VALUATION = "valuation"
    QUALITY = "quality"
    TECHNICAL = "technical"
    FUNDAMENTAL = "fundamental"


class RiskLevel(Enum):
    """Risk assessment levels"""
    LOW = "low"
    MODERATE = "moderate"
    HIGH = "high"
    VERY_HIGH = "very_high"


class TrendDirection(Enum):
    """Trend direction classification"""
    IMPROVING = "improving"
    STABLE = "stable"
    DETERIORATING = "deteriorating"
    VOLATILE = "volatile"


@dataclass
class AnalysisResult:
    """Standardized analysis result structure"""
    analysis_type: AnalysisType
    metric_name: str
    value: float
    interpretation: str
    risk_level: RiskLevel
    trend: TrendDirection = None
    benchmark_comparison: str = None
    industry_percentile: float = None
    quality_score: float = None
    methodology: str = None
    limitations: List[str] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)


@dataclass
class ComparativeAnalysis:
    """Multi-period or peer comparison results"""
    periods: List[str]
    values: List[float]
    trend_analysis: str
    volatility_measure: float
    growth_rate: float = None
    peer_comparison: Dict[str, float] = field(default_factory=dict)


@dataclass
class QualityAssessment:
    """Data and earnings quality assessment"""
    overall_score: float  # 0-100 scale
    earnings_quality: float
    balance_sheet_quality: float
    cash_flow_quality: float
    red_flags: List[str] = field(default_factory=list)
    warning_signs: List[str] = field(default_factory=list)
    quality_drivers: List[str] = field(default_factory=list)


class BaseAnalyzer(ABC):
    """
    Abstract base class for all financial statement analyzers.
    Implements CFA Institute analysis framework and best practices.
    """

    def __init__(self, enable_logging: bool = True):
        """Initialize base analyzer with common functionality"""
        self.logger = logging.getLogger(self.__class__.__name__) if enable_logging else None
        self._initialize_benchmarks()
        self._initialize_formulas()

    def _initialize_benchmarks(self):
        """Initialize industry benchmarks and thresholds"""
        # Standard liquidity benchmarks
        self.liquidity_benchmarks = {
            'current_ratio': {'excellent': 2.0, 'good': 1.5, 'adequate': 1.2, 'poor': 1.0},
            'quick_ratio': {'excellent': 1.5, 'good': 1.0, 'adequate': 0.8, 'poor': 0.5},
            'cash_ratio': {'excellent': 0.5, 'good': 0.3, 'adequate': 0.2, 'poor': 0.1}
        }

        # Standard activity benchmarks
        self.activity_benchmarks = {
            'asset_turnover': {'excellent': 2.0, 'good': 1.5, 'adequate': 1.0, 'poor': 0.5},
            'inventory_turnover': {'excellent': 12.0, 'good': 8.0, 'adequate': 6.0, 'poor': 4.0},
            'receivables_turnover': {'excellent': 12.0, 'good': 8.0, 'adequate': 6.0, 'poor': 4.0}
        }

        # Standard solvency benchmarks
        self.solvency_benchmarks = {
            'debt_to_equity': {'excellent': 0.3, 'good': 0.5, 'adequate': 1.0, 'poor': 2.0},
            'debt_to_assets': {'excellent': 0.2, 'good': 0.3, 'adequate': 0.5, 'poor': 0.7},
            'interest_coverage': {'excellent': 10.0, 'good': 5.0, 'adequate': 2.5, 'poor': 1.5}
        }

        # Standard profitability benchmarks
        self.profitability_benchmarks = {
            'gross_margin': {'excellent': 0.4, 'good': 0.3, 'adequate': 0.2, 'poor': 0.1},
            'operating_margin': {'excellent': 0.2, 'good': 0.15, 'adequate': 0.1, 'poor': 0.05},
            'net_margin': {'excellent': 0.15, 'good': 0.1, 'adequate': 0.05, 'poor': 0.02},
            'roe': {'excellent': 0.2, 'good': 0.15, 'adequate': 0.1, 'poor': 0.05},
            'roa': {'excellent': 0.15, 'good': 0.1, 'adequate': 0.05, 'poor': 0.02}
        }

        # Quality assessment thresholds
        self.quality_thresholds = {
            'earnings_quality': {'high': 80, 'moderate': 60, 'low': 40},
            'cash_flow_quality': {'high': 80, 'moderate': 60, 'low': 40},
            'balance_sheet_quality': {'high': 80, 'moderate': 60, 'low': 40}
        }

    def _initialize_formulas(self):
        """Initialize standard financial formulas and calculations"""
        # This will be extended by specific analyzers
        self.formula_registry = {}

    @abstractmethod
    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Main analysis method - must be implemented by subclasses

        Args:
            statements: Current period financial statements
            comparative_data: Historical data for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results
        """
        pass

    @abstractmethod
    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key metrics calculated by this analyzer"""
        pass

    def validate_data_sufficiency(self, statements: FinancialStatements,
                                  required_fields: List[str]) -> Tuple[bool, List[str]]:
        """
        Validate that required data fields are present for analysis

        Args:
            statements: Financial statements to validate
            required_fields: List of required field names

        Returns:
            Tuple of (is_sufficient, missing_fields)
        """
        missing_fields = []

        # Check across all statement types
        all_data = {
            **statements.income_statement,
            **statements.balance_sheet,
            **statements.cash_flow
        }

        for field in required_fields:
            if field not in all_data or all_data[field] is None:
                missing_fields.append(field)

        is_sufficient = len(missing_fields) == 0

        if not is_sufficient and self.logger:
            self.logger.warning(f"Missing required fields for analysis: {missing_fields}")

        return is_sufficient, missing_fields

    def calculate_trend(self, values: List[float], periods: List[str]) -> ComparativeAnalysis:
        """
        Calculate trend analysis for a series of values

        Args:
            values: List of metric values over time
            periods: List of period identifiers

        Returns:
            ComparativeAnalysis object with trend information
        """
        if len(values) < 2:
            return ComparativeAnalysis(
                periods=periods,
                values=values,
                trend_analysis="Insufficient data for trend analysis",
                volatility_measure=0.0
            )

        # Calculate growth rate (CAGR if multiple periods)
        if len(values) > 1:
            if values[0] != 0:
                if len(values) == 2:
                    growth_rate = (values[-1] / values[0]) - 1
                else:
                    n_periods = len(values) - 1
                    growth_rate = (values[-1] / values[0]) ** (1 / n_periods) - 1
            else:
                growth_rate = None
        else:
            growth_rate = None

        # Calculate volatility (coefficient of variation)
        mean_value = np.mean(values)
        std_value = np.std(values)
        volatility = std_value / mean_value if mean_value != 0 else 0

        # Determine trend direction
        if len(values) >= 3:
            recent_trend = np.polyfit(range(len(values)), values, 1)[0]
            if recent_trend > 0.05 * mean_value:
                trend_description = "Strong upward trend"
            elif recent_trend > 0.02 * mean_value:
                trend_description = "Moderate upward trend"
            elif recent_trend < -0.05 * mean_value:
                trend_description = "Strong downward trend"
            elif recent_trend < -0.02 * mean_value:
                trend_description = "Moderate downward trend"
            else:
                trend_description = "Stable trend"
        else:
            if values[-1] > values[0]:
                trend_description = "Improving"
            elif values[-1] < values[0]:
                trend_description = "Declining"
            else:
                trend_description = "Stable"

        return ComparativeAnalysis(
            periods=periods,
            values=values,
            trend_analysis=trend_description,
            volatility_measure=volatility,
            growth_rate=growth_rate
        )

    def assess_risk_level(self, metric_value: float, benchmark_dict: Dict[str, float],
                          higher_is_better: bool = True) -> RiskLevel:
        """
        Assess risk level based on metric value and benchmarks

        Args:
            metric_value: The calculated metric value
            benchmark_dict: Dictionary with benchmark thresholds
            higher_is_better: Whether higher values indicate better performance

        Returns:
            RiskLevel enum value
        """
        if higher_is_better:
            if metric_value >= benchmark_dict.get('excellent', float('inf')):
                return RiskLevel.LOW
            elif metric_value >= benchmark_dict.get('good', float('inf')):
                return RiskLevel.LOW
            elif metric_value >= benchmark_dict.get('adequate', float('inf')):
                return RiskLevel.MODERATE
            else:
                return RiskLevel.HIGH
        else:
            if metric_value <= benchmark_dict.get('excellent', 0):
                return RiskLevel.LOW
            elif metric_value <= benchmark_dict.get('good', 0):
                return RiskLevel.LOW
            elif metric_value <= benchmark_dict.get('adequate', float('inf')):
                return RiskLevel.MODERATE
            else:
                return RiskLevel.HIGH

    def generate_interpretation(self, metric_name: str, value: float,
                                risk_level: RiskLevel, analysis_type: AnalysisType) -> str:
        """
        Generate standardized interpretation text for metrics

        Args:
            metric_name: Name of the metric
            value: Calculated value
            risk_level: Assessed risk level
            analysis_type: Type of analysis

        Returns:
            Interpretation string
        """
        risk_descriptions = {
            RiskLevel.LOW: "strong",
            RiskLevel.MODERATE: "adequate",
            RiskLevel.HIGH: "weak",
            RiskLevel.VERY_HIGH: "very weak"
        }

        base_interpretation = f"The {metric_name} of {value:.3f} indicates {risk_descriptions[risk_level]} {analysis_type.value} performance."

        # Add specific insights based on metric type
        if analysis_type == AnalysisType.LIQUIDITY:
            if risk_level == RiskLevel.LOW:
                base_interpretation += " The company appears well-positioned to meet short-term obligations."
            elif risk_level == RiskLevel.HIGH:
                base_interpretation += " The company may face challenges meeting short-term obligations."

        elif analysis_type == AnalysisType.PROFITABILITY:
            if risk_level == RiskLevel.LOW:
                base_interpretation += " The company demonstrates effective management and competitive positioning."
            elif risk_level == RiskLevel.HIGH:
                base_interpretation += " The company may need to improve operational efficiency or pricing strategies."

        elif analysis_type == AnalysisType.SOLVENCY:
            if risk_level == RiskLevel.LOW:
                base_interpretation += " The company maintains conservative financial leverage."
            elif risk_level == RiskLevel.HIGH:
                base_interpretation += " The company carries significant financial risk due to high leverage."

        return base_interpretation

    def compare_to_industry(self, metric_value: float, industry_data: Optional[Dict]) -> Optional[str]:
        """
        Compare metric to industry benchmarks

        Args:
            metric_value: Company metric value
            industry_data: Industry benchmark data

        Returns:
            Comparison interpretation or None if no data available
        """
        if not industry_data:
            return None

        industry_median = industry_data.get('median')
        industry_q1 = industry_data.get('q1')
        industry_q3 = industry_data.get('q3')

        if not all([industry_median, industry_q1, industry_q3]):
            return None

        if metric_value >= industry_q3:
            return f"Above industry 75th percentile (Industry median: {industry_median:.3f})"
        elif metric_value >= industry_median:
            return f"Above industry median (Industry median: {industry_median:.3f})"
        elif metric_value >= industry_q1:
            return f"Below industry median but above 25th percentile (Industry median: {industry_median:.3f})"
        else:
            return f"Below industry 25th percentile (Industry median: {industry_median:.3f})"

    def calculate_percentile_rank(self, value: float, peer_values: List[float]) -> float:
        """Calculate percentile rank against peer group"""
        if not peer_values:
            return None

        rank = sum(1 for x in peer_values if x < value)
        percentile = (rank / len(peer_values)) * 100
        return percentile

    def assess_data_quality(self, statements: FinancialStatements,
                            required_fields: List[str]) -> QualityAssessment:
        """
        Assess the quality of financial data for analysis

        Args:
            statements: Financial statements to assess
            required_fields: Fields required for specific analysis

        Returns:
            QualityAssessment object
        """
        quality_issues = []
        warning_signs = []
        quality_drivers = []

        # Check data completeness
        all_data = {
            **statements.income_statement,
            **statements.balance_sheet,
            **statements.cash_flow
        }

        missing_critical = [field for field in required_fields if field not in all_data]
        if missing_critical:
            quality_issues.append(f"Missing critical fields: {missing_critical}")

        # Check for negative values where inappropriate
        negative_checks = {
            'revenue': statements.income_statement.get('revenue', 0),
            'total_assets': statements.balance_sheet.get('total_assets', 0),
            'cash_equivalents': statements.balance_sheet.get('cash_equivalents', 0)
        }

        for field, value in negative_checks.items():
            if value < 0:
                warning_signs.append(f"Negative {field}: {value}")

        # Check balance sheet equation
        assets = statements.balance_sheet.get('total_assets', 0)
        liabilities = statements.balance_sheet.get('total_liabilities', 0)
        equity = statements.balance_sheet.get('total_equity', 0)

        if abs(assets - (liabilities + equity)) > 0.01:
            quality_issues.append("Balance sheet equation does not balance")
        else:
            quality_drivers.append("Balance sheet equation balances correctly")

        # Check for unusual relationships
        current_assets = statements.balance_sheet.get('current_assets', 0)
        if current_assets > assets:
            warning_signs.append("Current assets exceed total assets")

        # Assess audit quality indicators
        if statements.period_info.audit_status == 'audited':
            quality_drivers.append("Financial statements are audited")
        elif statements.period_info.audit_status == 'unaudited':
            warning_signs.append("Financial statements are unaudited")

        # Calculate overall quality scores
        earnings_quality = max(0, 100 - len(quality_issues) * 20 - len(warning_signs) * 10)
        balance_sheet_quality = max(0, 100 - len(quality_issues) * 15 - len(warning_signs) * 8)
        cash_flow_quality = max(0, 100 - len(quality_issues) * 25 - len(warning_signs) * 12)

        overall_score = np.mean([earnings_quality, balance_sheet_quality, cash_flow_quality])

        return QualityAssessment(
            overall_score=overall_score,
            earnings_quality=earnings_quality,
            balance_sheet_quality=balance_sheet_quality,
            cash_flow_quality=cash_flow_quality,
            red_flags=quality_issues,
            warning_signs=warning_signs,
            quality_drivers=quality_drivers
        )

    def safe_divide(self, numerator: float, denominator: float, default: float = 0.0) -> float:
        """
        Safely perform division with handling for zero denominators

        Args:
            numerator: Numerator value
            denominator: Denominator value
            default: Default value to return if denominator is zero

        Returns:
            Division result or default value
        """
        if denominator == 0 or denominator is None:
            return default
        return numerator / denominator

    def format_percentage(self, value: float, decimal_places: int = 1) -> str:
        """Format decimal as percentage string"""
        return f"{value * 100:.{decimal_places}f}%"

    def format_ratio(self, value: float, decimal_places: int = 2) -> str:
        """Format ratio with specified decimal places"""
        return f"{value:.{decimal_places}f}"

    def get_analysis_limitations(self, statements: FinancialStatements) -> List[str]:
        """
        Identify general limitations that apply to the analysis

        Args:
            statements: Financial statements being analyzed

        Returns:
            List of limitation descriptions
        """
        limitations = []

        # Check reporting standard limitations
        if statements.company_info.reporting_standard == ReportingStandard.LOCAL_GAAP:
            limitations.append("Analysis based on local GAAP may not be comparable to IFRS or US GAAP companies")

        # Check data age
        period_end = statements.period_info.period_end
        days_old = (datetime.now().date() - period_end).days
        if days_old > 365:
            limitations.append(f"Financial data is {days_old} days old and may not reflect current conditions")

        # Check interim vs annual data
        if statements.period_info.period_type != 'annual':
            limitations.append("Analysis based on interim data may not reflect full-year performance")

        # Check audit status
        if statements.period_info.audit_status == 'unaudited':
            limitations.append("Unaudited financial statements may contain errors or adjustments")

        # Check data quality
        if statements.data_quality.get('completeness_score', 1.0) < 0.8:
            limitations.append("Incomplete financial data may affect analysis accuracy")

        return limitations

    def generate_recommendations(self, analysis_results: List[AnalysisResult]) -> List[str]:
        """
        Generate actionable recommendations based on analysis results

        Args:
            analysis_results: List of analysis results

        Returns:
            List of recommendation strings
        """
        recommendations = []

        # Analyze risk patterns
        high_risk_areas = [result for result in analysis_results
                           if result.risk_level in [RiskLevel.HIGH, RiskLevel.VERY_HIGH]]

        if high_risk_areas:
            for area in high_risk_areas:
                if area.analysis_type == AnalysisType.LIQUIDITY:
                    recommendations.append(
                        "Consider improving working capital management or securing additional credit facilities")
                elif area.analysis_type == AnalysisType.SOLVENCY:
                    recommendations.append("Focus on debt reduction and improving interest coverage ratios")
                elif area.analysis_type == AnalysisType.PROFITABILITY:
                    recommendations.append("Review cost structure and pricing strategies to improve profitability")
                elif area.analysis_type == AnalysisType.ACTIVITY:
                    recommendations.append("Optimize asset utilization and working capital efficiency")

        # Look for declining trends
        declining_trends = [result for result in analysis_results
                            if result.trend == TrendDirection.DETERIORATING]

        if declining_trends:
            recommendations.append("Monitor declining performance trends and develop corrective action plans")

        # Quality-based recommendations
        quality_issues = [result for result in analysis_results
                          if result.quality_score and result.quality_score < 60]

        if quality_issues:
            recommendations.append("Improve financial reporting quality and transparency")

        return list(set(recommendations))  # Remove duplicates