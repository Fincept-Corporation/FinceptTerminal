
"""
Financial Statement Comprehensive Analyzer Module
========================================

Comprehensive financial statement analysis and integration

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Management discussion and analysis sections
  - Auditor reports and financial statement footnotes
  - Industry benchmarks and competitor data
  - Economic indicators affecting financial performance

OUTPUT:
  - Financial analysis metrics and key performance indicators
  - Trend analysis and financial ratio calculations
  - Risk assessment and quality metrics
  - Comparative analysis and benchmarking results
  - Investment recommendations and insights

PARAMETERS:
  - analysis_period: Financial analysis period (default: 3 years)
  - industry_benchmark: Industry for comparative analysis (default: 'auto')
  - quality_threshold: Minimum financial quality score (default: 0.7)
  - growth_assumption: Growth rate assumption (default: 0.05)
  - currency: Reporting currency (default: 'USD')
"""


import numpy as np
from typing import Dict, List, Optional, Tuple, Union
from dataclasses import dataclass, field
from enum import Enum
import logging

# Import from core modules and statement analyzers
from ..core.base_analyzer import BaseAnalyzer, AnalysisResult, AnalysisType, RiskLevel, TrendDirection, \
    ComparativeAnalysis, QualityAssessment
from ..core.data_processor import FinancialStatements, ReportingStandard
from .income_statement import IncomeStatementAnalyzer
from .balance_sheet import BalanceSheetAnalyzer
from .cash_flow import CashFlowAnalyzer


class FinancialHealth(Enum):
    """Overall financial health classification"""
    EXCELLENT = "excellent"
    GOOD = "good"
    FAIR = "fair"
    POOR = "poor"
    DISTRESSED = "distressed"


class BusinessModel(Enum):
    """Business model classification based on financial patterns"""
    ASSET_HEAVY = "asset_heavy"
    ASSET_LIGHT = "asset_light"
    GROWTH = "growth"
    MATURE = "mature"
    TURNAROUND = "turnaround"
    CYCLICAL = "cyclical"


@dataclass
class IntegratedAnalysis:
    """Comprehensive integrated analysis results"""
    overall_financial_health: Optional[FinancialHealth]
    business_model_type: Optional[BusinessModel]
    key_strengths: List[str] = field(default_factory=list)
    key_weaknesses: List[str] = field(default_factory=list)
    critical_risks: List[str] = field(default_factory=list)
    strategic_recommendations: List[str] = field(default_factory=list)

    # Integrated scores
    liquidity_score: Optional[float] = None
    profitability_score: Optional[float] = None
    efficiency_score: Optional[float] = None
    leverage_score: Optional[float] = None
    growth_score: Optional[float] = None
    quality_score: Optional[float] = None

    # Overall composite score
    composite_score: Optional[float] = None


@dataclass
class StatementLinkages:
    """Analysis of relationships between financial statements"""
    income_to_cash_quality: float
    balance_sheet_efficiency: float
    working_capital_management: float
    capital_allocation_effectiveness: float
    earnings_sustainability: float

    # Red flags and quality issues
    reconciliation_issues: List[str] = field(default_factory=list)
    quality_concerns: List[str] = field(default_factory=list)
    positive_indicators: List[str] = field(default_factory=list)


@dataclass
class BusinessCycleAnalysis:
    """Analysis of where company is in business cycle"""
    lifecycle_stage: str
    growth_phase_indicators: List[str] = field(default_factory=list)
    maturity_indicators: List[str] = field(default_factory=list)
    decline_indicators: List[str] = field(default_factory=list)

    # Financial pattern analysis
    revenue_growth_pattern: str = ""
    profitability_pattern: str = ""
    cash_flow_pattern: str = ""
    investment_pattern: str = ""

class ComprehensiveAnalyzer(BaseAnalyzer):
    """
    Comprehensive financial statement analyzer that integrates all statement analyses.
    Provides holistic view of financial performance, position, and quality.

    Component scores are heuristic: each assessed metric contributes by its risk
    level (low 100, moderate 70, high 40, very high 20). A component with no
    assessed metric is reported as not available and the composite is re-weighted
    over the components that exist - missing data is never scored as a neutral 50.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)

        self.income_analyzer = IncomeStatementAnalyzer(enable_logging)
        self.balance_analyzer = BalanceSheetAnalyzer(enable_logging)
        self.cash_flow_analyzer = CashFlowAnalyzer(enable_logging)

        self._initialize_integration_weights()

    def _initialize_integration_weights(self):
        """Initialize weights for integrated scoring"""
        self.scoring_weights = {
            'liquidity': 0.2,
            'profitability': 0.25,
            'efficiency': 0.15,
            'leverage': 0.15,
            'growth': 0.15,
            'quality': 0.1
        }

        self.risk_weights = {
            RiskLevel.LOW: 100,
            RiskLevel.MODERATE: 70,
            RiskLevel.HIGH: 40,
            RiskLevel.VERY_HIGH: 20
        }

    # Descriptive lines whose risk level is fixed (composition, common-size, raw cash
    # flow amounts); they describe the company but are not assessments, so they are
    # kept out of the scores.
    INFORMATIONAL_METRICS = {
        "Current Asset Ratio", "PPE Ratio", "Investing Cash Flow", "Accounts Receivable Change",
        "Inventory Change", "Accounts Payable Change", "R&D Intensity", "Net Equity Activity",
        "Debt Financing Ratio",
    }

    @classmethod
    def _assessed(cls, results: List[AnalysisResult]) -> List[AnalysisResult]:
        """Results that carry a value and a genuine risk assessment."""
        return [r for r in results
                if r.value is not None and r.risk_level is not None
                and r.metric_name not in cls.INFORMATIONAL_METRICS
                and not r.metric_name.endswith((" as % of Revenue", " as % of Total Assets"))]

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive multi-statement analysis"""
        integrated_analysis, all_results = self._run(statements, comparative_data, industry_data)

        results = []
        results.extend(self._create_integrated_results(integrated_analysis, statements))
        results.extend(self._analyze_statement_linkages(statements, comparative_data))
        results.extend(self._analyze_business_cycle(statements, comparative_data))
        results.extend(self._perform_risk_assessment(all_results, statements))
        results.extend(self._generate_strategic_insights(all_results, statements, comparative_data))
        return results

    def _run(self, statements, comparative_data, industry_data):
        income_results = self.income_analyzer.analyze(statements, comparative_data, industry_data)
        balance_results = self.balance_analyzer.analyze(statements, comparative_data, industry_data)
        cash_flow_results = self.cash_flow_analyzer.analyze(statements, comparative_data, industry_data)
        all_results = income_results + balance_results + cash_flow_results
        integrated = self._perform_integrated_analysis(all_results, statements, comparative_data)
        return integrated, all_results

    def _perform_integrated_analysis(self, all_results: List[AnalysisResult],
                                     statements: FinancialStatements,
                                     comparative_data: Optional[
                                         List[FinancialStatements]] = None) -> IntegratedAnalysis:
        """Perform integrated analysis across all statements"""
        assessed = self._assessed(all_results)

        results_by_type = {}
        for result in assessed:
            results_by_type.setdefault(result.analysis_type, []).append(result)

        components = {
            'liquidity': self._calculate_component_score(results_by_type.get(AnalysisType.LIQUIDITY, [])),
            'profitability': self._calculate_component_score(results_by_type.get(AnalysisType.PROFITABILITY, [])),
            'efficiency': self._calculate_component_score(results_by_type.get(AnalysisType.ACTIVITY, [])),
            'leverage': self._calculate_component_score(results_by_type.get(AnalysisType.SOLVENCY, [])),
            'growth': self._calculate_growth_score(statements, comparative_data),
            'quality': self._calculate_component_score(results_by_type.get(AnalysisType.QUALITY, [])),
        }

        available = {k: v for k, v in components.items() if v is not None}
        # A composite from a single component (or a handful of metrics) would be presented as an
        # overall verdict it cannot support.
        if len(available) >= 2 and len(assessed) >= 4:
            weight_sum = sum(self.scoring_weights[k] for k in available)
            composite_score = sum(v * self.scoring_weights[k] for k, v in available.items()) / weight_sum
        else:
            composite_score = None

        financial_health = self._determine_financial_health(composite_score, assessed)
        business_model, model_reason = self._classify_business_model(statements, comparative_data)
        strengths, weaknesses = self._identify_strengths_weaknesses(assessed)
        critical_risks = self._identify_critical_risks(assessed)
        strategic_recommendations = self._generate_strategic_recommendations(assessed, statements)

        analysis = IntegratedAnalysis(
            overall_financial_health=financial_health,
            business_model_type=business_model,
            key_strengths=strengths,
            key_weaknesses=weaknesses,
            critical_risks=critical_risks,
            strategic_recommendations=strategic_recommendations,
            liquidity_score=components['liquidity'],
            profitability_score=components['profitability'],
            efficiency_score=components['efficiency'],
            leverage_score=components['leverage'],
            growth_score=components['growth'],
            quality_score=components['quality'],
            composite_score=composite_score
        )
        analysis.business_model_reason = model_reason
        analysis.components_used = sorted(available)
        return analysis

    def _calculate_component_score(self, results: List[AnalysisResult]) -> Optional[float]:
        """Mean risk-weight score of the assessed metrics; None when there are none."""
        results = self._assessed(results)
        if not results:
            return None
        return sum(self.risk_weights[r.risk_level] for r in results) / len(results)

    def _calculate_growth_score(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> Optional[float]:
        """Growth score from revenue, net income and asset growth vs the prior period;
        None without history (no placeholder)."""
        if not comparative_data:
            return None

        growth_factors = []
        revenue_growth = self.growth(statements.income_statement.get('revenue'),
                                     self.prior_value(comparative_data, 'income_statement', 'revenue'))
        if revenue_growth is not None:
            growth_factors.append(min(100, max(0, 50 + revenue_growth * 200)))

        ni_growth = self.growth(statements.income_statement.get('net_income'),
                                self.prior_value(comparative_data, 'income_statement', 'net_income'))
        if ni_growth is not None:
            growth_factors.append(min(100, max(0, 50 + ni_growth * 200)))

        asset_growth = self.growth(statements.balance_sheet.get('total_assets'),
                                   self.prior_value(comparative_data, 'balance_sheet', 'total_assets'))
        if asset_growth is not None:
            growth_factors.append(min(100, max(0, 50 + asset_growth * 150)))

        return float(np.mean(growth_factors)) if growth_factors else None

    def _determine_financial_health(self, composite_score: Optional[float],
                                    all_results: List[AnalysisResult]) -> Optional[FinancialHealth]:
        """Determine overall financial health classification"""
        if composite_score is None:
            return None

        high_risk_count = sum(1 for result in all_results if result.risk_level == RiskLevel.HIGH)
        very_high_risk_count = sum(1 for result in all_results if result.risk_level == RiskLevel.VERY_HIGH)
        if very_high_risk_count > 2 or high_risk_count > 5:
            return FinancialHealth.DISTRESSED

        if composite_score >= 85:
            return FinancialHealth.EXCELLENT
        elif composite_score >= 70:
            return FinancialHealth.GOOD
        elif composite_score >= 55:
            return FinancialHealth.FAIR
        elif composite_score >= 40:
            return FinancialHealth.POOR
        return FinancialHealth.DISTRESSED

    def _classify_business_model(self, statements: FinancialStatements,
                                 comparative_data: Optional[List[FinancialStatements]] = None
                                 ) -> Tuple[Optional[BusinessModel], str]:
        """Classify business model; always returns (model or None, reason)."""
        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        total_assets = balance_sheet.get('total_assets')
        revenue = income_statement.get('revenue')
        net_income = income_statement.get('net_income')
        ppe_net = balance_sheet.get('ppe_net')

        if not total_assets or total_assets <= 0 or not revenue or revenue <= 0:
            return None, "total assets and revenue (both positive) are required"

        asset_intensity = total_assets / revenue
        ppe_ratio = ppe_net / total_assets if ppe_net is not None else None

        revenue_growth = self.growth(revenue, self.prior_value(comparative_data, 'income_statement', 'revenue'))
        is_growing = revenue_growth is not None and revenue_growth > 0.1

        if asset_intensity > 2.0 or (ppe_ratio is not None and ppe_ratio > 0.4):
            return BusinessModel.ASSET_HEAVY, f"assets/revenue {asset_intensity:.2f}" + (
                f", net PPE/assets {ppe_ratio:.2f}" if ppe_ratio is not None else "")
        if asset_intensity < 0.8 and ppe_ratio is not None and ppe_ratio < 0.2:
            return BusinessModel.ASSET_LIGHT, f"assets/revenue {asset_intensity:.2f}, net PPE/assets {ppe_ratio:.2f}"
        if net_income is None:
            return None, "net income not reported"
        if is_growing and net_income > 0:
            return BusinessModel.GROWTH, f"revenue growth {revenue_growth:.1%} with positive earnings"
        if net_income <= 0 and comparative_data:
            loss_periods = sum(1 for p in comparative_data[-3:]
                               if p.income_statement.get('net_income') is not None
                               and p.income_statement['net_income'] < 0)
            if loss_periods >= 2:
                return BusinessModel.TURNAROUND, f"loss now and in {loss_periods} of the last 3 prior periods"
            return BusinessModel.CYCLICAL, "loss after mostly profitable prior periods"
        if net_income > 0 and not is_growing:
            return BusinessModel.MATURE, "profitable, revenue growth at or below 10%"
        return BusinessModel.CYCLICAL, "loss-making, no history to separate cyclical from structural losses"

    def _identify_strengths_weaknesses(self, all_results: List[AnalysisResult]) -> Tuple[List[str], List[str]]:
        """Identify key strengths and weaknesses from assessed results"""
        strengths = []
        weaknesses = []

        by_type_risk = {}
        for result in all_results:
            by_type_risk.setdefault((result.analysis_type, result.risk_level), []).append(result)

        strength_text = {
            AnalysisType.LIQUIDITY: "Strong liquidity position with adequate cash resources",
            AnalysisType.PROFITABILITY: "Robust profitability across multiple metrics",
            AnalysisType.SOLVENCY: "Conservative financial leverage and strong solvency",
            AnalysisType.ACTIVITY: "Efficient asset utilization and operational management",
            AnalysisType.QUALITY: "High quality financial reporting and earnings",
        }
        weakness_text = {
            AnalysisType.LIQUIDITY: "Liquidity concerns - potential difficulty meeting short-term obligations",
            AnalysisType.PROFITABILITY: "Weak profitability performance requiring operational improvement",
            AnalysisType.SOLVENCY: "High financial leverage creating elevated financial risk",
            AnalysisType.ACTIVITY: "Inefficient asset utilization and operational inefficiencies",
            AnalysisType.QUALITY: "Financial reporting quality concerns requiring investigation",
        }

        for (analysis_type, risk_level), results in by_type_risk.items():
            # A strength needs at least two low-risk metrics and no high-risk metric in the category
            has_high = any((analysis_type, lvl) in by_type_risk for lvl in (RiskLevel.HIGH, RiskLevel.VERY_HIGH))
            if risk_level == RiskLevel.LOW and len(results) >= 2 and not has_high and analysis_type in strength_text:
                strengths.append(strength_text[analysis_type])
            if risk_level in (RiskLevel.HIGH, RiskLevel.VERY_HIGH) and analysis_type in weakness_text:
                names = ", ".join(r.metric_name for r in results[:4])
                text = f"{weakness_text[analysis_type]} ({names})"
                if text not in weaknesses:
                    weaknesses.append(text)

        return strengths, weaknesses

    def _identify_critical_risks(self, all_results: List[AnalysisResult]) -> List[str]:
        """Identify critical risks from assessed results"""
        critical_risks = []

        for risk in (r for r in all_results if r.risk_level == RiskLevel.VERY_HIGH):
            critical_risks.append(f"Critical: {risk.metric_name} - {risk.interpretation}")

        high_risks_by_type = {}
        for result in all_results:
            if result.risk_level == RiskLevel.HIGH:
                high_risks_by_type.setdefault(result.analysis_type, []).append(result)
        for analysis_type, risks in high_risks_by_type.items():
            if len(risks) >= 2:
                critical_risks.append(f"Multiple high-risk {analysis_type.value} indicators require immediate attention")

        liquidity_risks = [r for r in all_results if r.analysis_type == AnalysisType.LIQUIDITY
                           and r.risk_level in (RiskLevel.HIGH, RiskLevel.VERY_HIGH)]
        solvency_risks = [r for r in all_results if r.analysis_type == AnalysisType.SOLVENCY
                          and r.risk_level in (RiskLevel.HIGH, RiskLevel.VERY_HIGH)]
        if liquidity_risks and solvency_risks:
            critical_risks.append("Combined liquidity and solvency risks create financial distress potential")

        return critical_risks

    def _generate_strategic_recommendations(self, all_results: List[AnalysisResult],
                                            statements: FinancialStatements) -> List[str]:
        """Generate strategic recommendations based on assessed results"""
        recommendations = []
        elevated = (RiskLevel.HIGH, RiskLevel.MODERATE, RiskLevel.VERY_HIGH)

        def flagged(analysis_type):
            return any(r.analysis_type == analysis_type and r.risk_level in elevated for r in all_results)

        if flagged(AnalysisType.LIQUIDITY):
            recommendations.append("Improve working capital management and consider establishing credit facilities")
        if flagged(AnalysisType.PROFITABILITY):
            recommendations.append("Focus on cost optimization and revenue enhancement strategies")
        if flagged(AnalysisType.ACTIVITY):
            recommendations.append("Optimize asset utilization and improve operational efficiency")
        if flagged(AnalysisType.SOLVENCY):
            recommendations.append("Consider debt reduction and strengthen balance sheet structure")
        if flagged(AnalysisType.QUALITY):
            recommendations.append("Enhance financial reporting transparency and earnings quality")

        net_income = statements.income_statement.get('net_income')
        if net_income is not None:
            if net_income > 0:
                recommendations.append("Consider strategic investments for sustainable growth")
            else:
                recommendations.append("Develop turnaround strategy to restore profitability")

        return recommendations

    def _create_integrated_results(self, integrated_analysis: IntegratedAnalysis,
                                   statements: FinancialStatements) -> List[AnalysisResult]:
        """Convert integrated analysis to AnalysisResult format"""
        results = []
        components_used = getattr(integrated_analysis, 'components_used', [])

        if integrated_analysis.composite_score is None:
            results.append(self.unavailable(AnalysisType.QUALITY, "Overall Financial Health",
                                            "fewer than 2 components (or fewer than 4 assessed metrics) "
                                            "could be scored from the supplied data"))
        else:
            health = integrated_analysis.overall_financial_health
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Overall Financial Health",
                value=integrated_analysis.composite_score,
                interpretation=(f"Overall financial health is {health.value} with composite score of "
                                f"{integrated_analysis.composite_score:.1f} (components scored: {', '.join(components_used)})"),
                risk_level=RiskLevel.LOW if health in (FinancialHealth.EXCELLENT, FinancialHealth.GOOD) else RiskLevel.HIGH,
                methodology="Weighted composite of the available component scores (weights re-normalised over the components that could be scored)",
                label=health.value
            ))

        model = integrated_analysis.business_model_type
        reason = getattr(integrated_analysis, 'business_model_reason', '')
        if model is None:
            results.append(self.unavailable(AnalysisType.QUALITY, "Business Model Type", reason,
                                            "Classification based on asset intensity, growth patterns, and profitability"))
        else:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Business Model Type",
                value=None,
                interpretation=f"Business model classified as {model.value} ({reason})",
                risk_level=RiskLevel.LOW,
                methodology="Classification based on asset intensity, growth patterns, and profitability",
                label=model.value
            ))

        component_scores = {
            "Liquidity Score": integrated_analysis.liquidity_score,
            "Profitability Score": integrated_analysis.profitability_score,
            "Efficiency Score": integrated_analysis.efficiency_score,
            "Leverage Score": integrated_analysis.leverage_score,
            "Growth Score": integrated_analysis.growth_score,
            "Quality Score": integrated_analysis.quality_score
        }
        for score_name, score_value in component_scores.items():
            if score_value is None:
                reason = ("no prior period supplied (pass 'history')" if score_name == "Growth Score"
                          else "no metric in this category could be computed from the supplied data")
                results.append(self.unavailable(AnalysisType.QUALITY, score_name, reason))
                continue
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name=score_name,
                value=score_value,
                interpretation=f"{score_name}: {score_value:.1f}/100",
                risk_level=RiskLevel.LOW if score_value > 70 else RiskLevel.MODERATE if score_value > 50 else RiskLevel.HIGH,
                methodology="Mean of metric risk weights in the category (low 100, moderate 70, high 40, very high 20)"
                if score_name != "Growth Score" else "Mean of scaled revenue, net income and asset growth vs prior period"
            ))

        if integrated_analysis.key_strengths:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Key Strengths",
                value=len(integrated_analysis.key_strengths),
                interpretation="; ".join(integrated_analysis.key_strengths),
                risk_level=RiskLevel.LOW,
                recommendations=integrated_analysis.key_strengths
            ))

        if integrated_analysis.key_weaknesses:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Key Weaknesses",
                value=len(integrated_analysis.key_weaknesses),
                interpretation="; ".join(integrated_analysis.key_weaknesses),
                risk_level=RiskLevel.HIGH,
                limitations=integrated_analysis.key_weaknesses
            ))

        if integrated_analysis.critical_risks:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Critical Risks",
                value=len(integrated_analysis.critical_risks),
                interpretation="; ".join(integrated_analysis.critical_risks),
                risk_level=RiskLevel.HIGH,
                limitations=integrated_analysis.critical_risks
            ))

        if integrated_analysis.strategic_recommendations:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Strategic Recommendations",
                value=len(integrated_analysis.strategic_recommendations),
                interpretation="; ".join(integrated_analysis.strategic_recommendations),
                risk_level=RiskLevel.LOW,
                recommendations=integrated_analysis.strategic_recommendations
            ))

        return results

    def _analyze_statement_linkages(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze relationships and linkages between financial statements"""
        results = []
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        balance_sheet = statements.balance_sheet

        net_income = income_statement.get('net_income')
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        if net_income is not None and operating_cash_flow is not None:
            if net_income > 0:
                cash_quality_ratio = operating_cash_flow / net_income
                cash_quality_interpretation = "Excellent cash conversion from earnings" if cash_quality_ratio > 1.2 else "Good cash quality" if cash_quality_ratio > 1.0 else "Moderate cash quality" if cash_quality_ratio > 0.8 else "Poor cash conversion quality"
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Earnings-Cash Flow Quality",
                    value=cash_quality_ratio,
                    interpretation=cash_quality_interpretation,
                    risk_level=RiskLevel.LOW if cash_quality_ratio > 1.0 else RiskLevel.MODERATE if cash_quality_ratio > 0.7 else RiskLevel.HIGH,
                    methodology="Operating Cash Flow / Net Income"
                ))
            else:
                results.append(self.unavailable(AnalysisType.QUALITY, "Earnings-Cash Flow Quality",
                                                f"net income is not positive ({net_income:,.2f}); "
                                                f"operating cash flow {operating_cash_flow:,.2f}",
                                                "Operating Cash Flow / Net Income"))

        revenue = income_statement.get('revenue')
        avg_assets, basis = self.average_balance(statements, comparative_data, 'total_assets')
        if revenue is not None and revenue > 0 and avg_assets and avg_assets > 0:
            asset_turnover = revenue / avg_assets
            efficiency_interpretation = "High asset efficiency" if asset_turnover > 1.5 else "Moderate asset efficiency" if asset_turnover > 1.0 else "Low asset efficiency"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Statement Integration - Asset Efficiency",
                value=asset_turnover,
                interpretation=efficiency_interpretation,
                risk_level=RiskLevel.LOW if asset_turnover > 1.2 else RiskLevel.MODERATE if asset_turnover > 0.8 else RiskLevel.HIGH,
                methodology=f"Revenue (IS) / Total Assets (BS, {self.basis_note(basis)})"
            ))

        current_assets = balance_sheet.get('current_assets')
        current_liabilities = balance_sheet.get('current_liabilities')
        working_capital_change = cash_flow.get('working_capital_change')
        if current_assets is not None and current_liabilities is not None and working_capital_change:
            working_capital = current_assets - current_liabilities
            if working_capital != 0:
                wc_efficiency = abs(working_capital_change) / abs(working_capital)
                wc_interpretation = "Significant working capital volatility" if wc_efficiency > 0.2 else "Moderate working capital changes" if wc_efficiency > 0.1 else "Stable working capital management"
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name="Working Capital Management Integration",
                    value=wc_efficiency,
                    interpretation=wc_interpretation,
                    risk_level=RiskLevel.HIGH if wc_efficiency > 0.3 else RiskLevel.MODERATE if wc_efficiency > 0.15 else RiskLevel.LOW,
                    methodology="|WC Change (CF)| / |Net Working Capital (BS)|"
                ))

        return results

    def _analyze_business_cycle(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze where company is in business cycle (needs 2+ prior periods)"""
        results = []
        if not comparative_data or len(comparative_data) < 2:
            return results

        periods = list(comparative_data) + [statements]
        ni_values = [p.income_statement['net_income'] for p in periods if 'net_income' in p.income_statement]
        revenue = statements.income_statement.get('revenue')
        recent_revenue_growth = self.growth(revenue, self.prior_value(comparative_data, 'income_statement', 'revenue'))

        if recent_revenue_growth is None or len(ni_values) < 2:
            results.append(self.unavailable(AnalysisType.QUALITY, "Business Lifecycle Stage",
                                            "revenue for the last two periods and net income history are required"))
            return results

        lifecycle_indicators = []
        if recent_revenue_growth > 0.15:
            lifecycle_indicators.append("High revenue growth indicates growth stage")
        elif recent_revenue_growth > 0.05:
            lifecycle_indicators.append("Moderate growth suggests expansion phase")
        elif recent_revenue_growth < -0.05:
            lifecycle_indicators.append("Declining revenue suggests maturity or decline phase")
        else:
            lifecycle_indicators.append("Stable revenue indicates mature stage")

        profitability_ratio = sum(1 for ni in ni_values if ni > 0) / len(ni_values)
        if profitability_ratio > 0.8:
            lifecycle_indicators.append("Consistent profitability indicates mature business model")
        elif profitability_ratio < 0.5:
            lifecycle_indicators.append("Inconsistent profitability suggests early stage or turnaround situation")

        if recent_revenue_growth > 0.2 and profitability_ratio > 0.6:
            lifecycle_stage = "Growth Stage"
        elif recent_revenue_growth > 0.05 and profitability_ratio > 0.7:
            lifecycle_stage = "Expansion Stage"
        elif abs(recent_revenue_growth) < 0.05 and profitability_ratio > 0.8:
            lifecycle_stage = "Mature Stage"
        elif recent_revenue_growth < -0.1 or profitability_ratio < 0.4:
            lifecycle_stage = "Decline/Turnaround Stage"
        else:
            lifecycle_stage = "Transition Stage"

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Business Lifecycle Stage",
            value=None,
            interpretation=f"Company appears to be in {lifecycle_stage}",
            risk_level=RiskLevel.LOW,
            recommendations=lifecycle_indicators,
            methodology="Analysis of revenue growth and profitability trends over time",
            label=lifecycle_stage
        ))
        return results

    def _perform_risk_assessment(self, all_results: List[AnalysisResult],
                                 statements: FinancialStatements) -> List[AnalysisResult]:
        """Share of assessed metrics at high / very high risk"""
        results = []
        assessed = self._assessed(all_results)
        if not assessed:
            results.append(self.unavailable(AnalysisType.QUALITY, "Overall Risk Assessment",
                                            "no metric could be assessed"))
            return results

        high = [r for r in assessed if r.risk_level in (RiskLevel.HIGH, RiskLevel.VERY_HIGH)]
        high_risk_ratio = len(high) / len(assessed)
        if high_risk_ratio > 0.3:
            overall_risk, risk_level = "High Risk", RiskLevel.HIGH
        elif high_risk_ratio > 0.15:
            overall_risk, risk_level = "Moderate Risk", RiskLevel.MODERATE
        else:
            overall_risk, risk_level = "Low Risk", RiskLevel.LOW

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Overall Risk Assessment",
            value=high_risk_ratio,
            interpretation=f"{overall_risk} - {high_risk_ratio:.1%} of {len(assessed)} assessed metrics show elevated risk",
            risk_level=risk_level,
            methodology="Proportion of assessed metrics at high or very high risk"
        ))

        risk_by_type = {}
        for result in high:
            risk_by_type[result.analysis_type] = risk_by_type.get(result.analysis_type, 0) + 1
        if risk_by_type:
            max_risk_type = max(risk_by_type, key=risk_by_type.get)
            max_risk_count = risk_by_type[max_risk_type]
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Risk Concentration",
                value=max_risk_count,
                interpretation=f"Highest risk concentration in {max_risk_type.value} with {max_risk_count} high-risk metrics",
                risk_level=RiskLevel.HIGH if max_risk_count > 2 else RiskLevel.MODERATE,
                methodology="Analysis of risk distribution across financial areas"
            ))

        return results

    def _generate_strategic_insights(self, all_results: List[AnalysisResult],
                                     statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Generate high-level strategic insights"""
        results = []
        cash_flow = statements.cash_flow

        # Capital deployment (all outflows are positive magnitudes)
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        deployed = [cash_flow[k] for k in ('capex', 'dividends_paid', 'acquisitions', 'equity_repurchased')
                    if k in cash_flow]
        total_capital_deployment = sum(deployed)
        if operating_cash_flow and operating_cash_flow > 0 and total_capital_deployment > 0:
            capital_efficiency = total_capital_deployment / operating_cash_flow
            capital_interpretation = "Aggressive capital deployment - exceeds operating cash flow" if capital_efficiency > 1.0 else "Balanced capital allocation" if capital_efficiency > 0.7 else "Conservative capital deployment"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Capital Allocation Strategy",
                value=capital_efficiency,
                interpretation=capital_interpretation,
                risk_level=RiskLevel.MODERATE if capital_efficiency > 1.2 else RiskLevel.LOW,
                methodology="(CapEx + Dividends + Acquisitions + Buybacks) / Operating Cash Flow"
            ))

        # Gross margin as a pricing-power proxy (only when gross profit is known)
        income_statement = statements.income_statement
        revenue = income_statement.get('revenue')
        gross_profit = self.gross_profit(income_statement)
        if revenue is not None and revenue > 0 and gross_profit is not None:
            gross_margin = gross_profit / revenue
            competitive_strength = "Strong competitive position" if gross_margin > 0.4 else "Moderate competitive position" if gross_margin > 0.2 else "Weak competitive position"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Competitive Position Indicator",
                value=gross_margin,
                interpretation=competitive_strength,
                risk_level=RiskLevel.LOW if gross_margin > 0.3 else RiskLevel.MODERATE if gross_margin > 0.15 else RiskLevel.HIGH,
                methodology="Gross margin as proxy for competitive strength and pricing power"
            ))

        return results

    def get_key_metrics(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        unavailable: Optional[Dict[str, str]] = None) -> Dict[str, float]:
        """Return comprehensive key metrics from all analyzers"""
        income_metrics = self.income_analyzer.get_key_metrics(statements, comparative_data, unavailable)
        balance_metrics = self.balance_analyzer.get_key_metrics(statements, comparative_data, unavailable)
        cash_flow_metrics = self.cash_flow_analyzer.get_key_metrics(statements, comparative_data, unavailable)
        return {**income_metrics, **balance_metrics, **cash_flow_metrics}

    def create_integrated_analysis(self, statements: FinancialStatements,
                                   comparative_data: Optional[List[FinancialStatements]] = None,
                                   industry_data: Optional[Dict] = None) -> IntegratedAnalysis:
        """Integrated analysis object computed from the statements."""
        integrated, _ = self._run(statements, comparative_data, industry_data)
        return integrated
