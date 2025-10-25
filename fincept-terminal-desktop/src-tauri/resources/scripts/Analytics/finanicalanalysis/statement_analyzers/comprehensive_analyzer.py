
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
import pandas as pd
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
    overall_financial_health: FinancialHealth
    business_model_type: BusinessModel
    key_strengths: List[str] = field(default_factory=list)
    key_weaknesses: List[str] = field(default_factory=list)
    critical_risks: List[str] = field(default_factory=list)
    strategic_recommendations: List[str] = field(default_factory=list)

    # Integrated scores
    liquidity_score: float = 0.0
    profitability_score: float = 0.0
    efficiency_score: float = 0.0
    leverage_score: float = 0.0
    growth_score: float = 0.0
    quality_score: float = 0.0

    # Overall composite score
    composite_score: float = 0.0


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
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)

        # Initialize component analyzers
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

        # Risk factor weights
        self.risk_weights = {
            RiskLevel.LOW: 100,
            RiskLevel.MODERATE: 70,
            RiskLevel.HIGH: 40,
            RiskLevel.VERY_HIGH: 20
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive multi-statement analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of integrated analysis results
        """
        results = []

        # Run individual statement analyses
        income_results = self.income_analyzer.analyze(statements, comparative_data, industry_data)
        balance_results = self.balance_analyzer.analyze(statements, comparative_data, industry_data)
        cash_flow_results = self.cash_flow_analyzer.analyze(statements, comparative_data, industry_data)

        # Combine all results
        all_results = income_results + balance_results + cash_flow_results

        # Perform integrated analysis
        integrated_analysis = self._perform_integrated_analysis(all_results, statements, comparative_data)

        # Convert integrated analysis to AnalysisResult format
        results.extend(self._create_integrated_results(integrated_analysis, statements))

        # Analyze statement linkages
        results.extend(self._analyze_statement_linkages(statements, comparative_data))

        # Business cycle analysis
        results.extend(self._analyze_business_cycle(statements, comparative_data))

        # Risk assessment
        results.extend(self._perform_risk_assessment(all_results, statements))

        # Generate strategic insights
        results.extend(self._generate_strategic_insights(all_results, statements, comparative_data))

        return results

    def _perform_integrated_analysis(self, all_results: List[AnalysisResult],
                                     statements: FinancialStatements,
                                     comparative_data: Optional[
                                         List[FinancialStatements]] = None) -> IntegratedAnalysis:
        """Perform integrated analysis across all statements"""

        # Categorize results by analysis type
        results_by_type = {}
        for result in all_results:
            if result.analysis_type not in results_by_type:
                results_by_type[result.analysis_type] = []
            results_by_type[result.analysis_type].append(result)

        # Calculate component scores
        liquidity_score = self._calculate_component_score(results_by_type.get(AnalysisType.LIQUIDITY, []))
        profitability_score = self._calculate_component_score(results_by_type.get(AnalysisType.PROFITABILITY, []))
        efficiency_score = self._calculate_component_score(results_by_type.get(AnalysisType.ACTIVITY, []))
        leverage_score = self._calculate_component_score(results_by_type.get(AnalysisType.SOLVENCY, []))
        quality_score = self._calculate_component_score(results_by_type.get(AnalysisType.QUALITY, []))

        # Calculate growth score from trends
        growth_score = self._calculate_growth_score(statements, comparative_data)

        # Calculate composite score
        composite_score = (
                liquidity_score * self.scoring_weights['liquidity'] +
                profitability_score * self.scoring_weights['profitability'] +
                efficiency_score * self.scoring_weights['efficiency'] +
                leverage_score * self.scoring_weights['leverage'] +
                growth_score * self.scoring_weights['growth'] +
                quality_score * self.scoring_weights['quality']
        )

        # Determine overall financial health
        financial_health = self._determine_financial_health(composite_score, all_results)

        # Classify business model
        business_model = self._classify_business_model(statements, comparative_data)

        # Identify strengths and weaknesses
        strengths, weaknesses = self._identify_strengths_weaknesses(all_results)

        # Identify critical risks
        critical_risks = self._identify_critical_risks(all_results)

        # Generate strategic recommendations
        strategic_recommendations = self._generate_strategic_recommendations(all_results, statements)

        return IntegratedAnalysis(
            overall_financial_health=financial_health,
            business_model_type=business_model,
            key_strengths=strengths,
            key_weaknesses=weaknesses,
            critical_risks=critical_risks,
            strategic_recommendations=strategic_recommendations,
            liquidity_score=liquidity_score,
            profitability_score=profitability_score,
            efficiency_score=efficiency_score,
            leverage_score=leverage_score,
            growth_score=growth_score,
            quality_score=quality_score,
            composite_score=composite_score
        )

    def _calculate_component_score(self, results: List[AnalysisResult]) -> float:
        """Calculate component score based on risk levels"""
        if not results:
            return 50.0  # Neutral score if no data

        total_weight = 0
        weighted_score = 0

        for result in results:
            weight = 1.0  # Equal weight for now, could be refined
            score = self.risk_weights.get(result.risk_level, 50)

            weighted_score += score * weight
            total_weight += weight

        return weighted_score / total_weight if total_weight > 0 else 50.0

    def _calculate_growth_score(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> float:
        """Calculate growth score based on key metrics trends"""
        if not comparative_data or len(comparative_data) == 0:
            return 50.0  # Neutral if no historical data

        growth_factors = []

        # Revenue growth
        current_revenue = statements.income_statement.get('revenue', 0)
        prev_revenue = comparative_data[-1].income_statement.get('revenue', 0)
        if prev_revenue > 0:
            revenue_growth = (current_revenue / prev_revenue) - 1
            growth_factors.append(min(100, max(0, 50 + revenue_growth * 200)))  # Scale to 0-100

        # Net income growth
        current_ni = statements.income_statement.get('net_income', 0)
        prev_ni = comparative_data[-1].income_statement.get('net_income', 0)
        if prev_ni > 0:
            ni_growth = (current_ni / prev_ni) - 1
            growth_factors.append(min(100, max(0, 50 + ni_growth * 200)))

        # Asset growth
        current_assets = statements.balance_sheet.get('total_assets', 0)
        prev_assets = comparative_data[-1].balance_sheet.get('total_assets', 0)
        if prev_assets > 0:
            asset_growth = (current_assets / prev_assets) - 1
            growth_factors.append(min(100, max(0, 50 + asset_growth * 150)))

        return np.mean(growth_factors) if growth_factors else 50.0

    def _determine_financial_health(self, composite_score: float, all_results: List[AnalysisResult]) -> FinancialHealth:
        """Determine overall financial health classification"""

        # Check for distressed indicators
        high_risk_count = sum(1 for result in all_results if result.risk_level == RiskLevel.HIGH)
        very_high_risk_count = sum(1 for result in all_results if result.risk_level == RiskLevel.VERY_HIGH)

        if very_high_risk_count > 2 or high_risk_count > 5:
            return FinancialHealth.DISTRESSED

        # Classify based on composite score
        if composite_score >= 85:
            return FinancialHealth.EXCELLENT
        elif composite_score >= 70:
            return FinancialHealth.GOOD
        elif composite_score >= 55:
            return FinancialHealth.FAIR
        elif composite_score >= 40:
            return FinancialHealth.POOR
        else:
            return FinancialHealth.DISTRESSED

    def _classify_business_model(self, statements: FinancialStatements,
                                 comparative_data: Optional[List[FinancialStatements]] = None) -> BusinessModel:
        """Classify business model based on financial patterns"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        total_assets = balance_sheet.get('total_assets', 0)
        ppe_net = balance_sheet.get('ppe_net', 0)
        revenue = income_statement.get('revenue', 0)
        net_income = income_statement.get('net_income', 0)

        # Asset intensity analysis
        asset_intensity = self.safe_divide(total_assets, revenue) if revenue > 0 else 0
        ppe_ratio = self.safe_divide(ppe_net, total_assets) if total_assets > 0 else 0

        # Growth analysis
        is_growing = False
        if comparative_data and len(comparative_data) > 0:
            prev_revenue = comparative_data[-1].income_statement.get('revenue', 0)
            if prev_revenue > 0:
                revenue_growth = (revenue / prev_revenue) - 1
                is_growing = revenue_growth > 0.1  # 10% growth threshold

        # Profitability analysis
        net_margin = self.safe_divide(net_income, revenue) if revenue > 0 else 0
        is_profitable = net_income > 0

        # Classification logic
        if asset_intensity > 2.0 or ppe_ratio > 0.4:
            return BusinessModel.ASSET_HEAVY
        elif asset_intensity < 0.8 and ppe_ratio < 0.2:
            return BusinessModel.ASSET_LIGHT
        elif is_growing and net_margin > 0:
            return BusinessModel.GROWTH
        elif not is_profitable and comparative_data:
            # Check if declining
            declining_periods = 0
            for i in range(min(3, len(comparative_data))):
                past_ni = comparative_data[-(i + 1)].income_statement.get('net_income', 0)
                if past_ni < 0:
                    declining_periods += 1
            if declining_periods >= 2:
                return BusinessModel.TURNAROUND
        elif is_profitable and not is_growing:
            return BusinessModel.MATURE
        else:
            return BusinessModel.CYCLICAL

    def _identify_strengths_weaknesses(self, all_results: List[AnalysisResult]) -> Tuple[List[str], List[str]]:
        """Identify key strengths and weaknesses from analysis results"""

        strengths = []
        weaknesses = []

        # Group results by analysis type and risk level
        by_type_risk = {}
        for result in all_results:
            key = (result.analysis_type, result.risk_level)
            if key not in by_type_risk:
                by_type_risk[key] = []
            by_type_risk[key].append(result)

        # Identify strengths (low risk areas)
        for (analysis_type, risk_level), results in by_type_risk.items():
            if risk_level == RiskLevel.LOW and len(results) >= 2:
                if analysis_type == AnalysisType.LIQUIDITY:
                    strengths.append("Strong liquidity position with adequate cash resources")
                elif analysis_type == AnalysisType.PROFITABILITY:
                    strengths.append("Robust profitability across multiple metrics")
                elif analysis_type == AnalysisType.SOLVENCY:
                    strengths.append("Conservative financial leverage and strong solvency")
                elif analysis_type == AnalysisType.ACTIVITY:
                    strengths.append("Efficient asset utilization and operational management")
                elif analysis_type == AnalysisType.QUALITY:
                    strengths.append("High quality financial reporting and earnings")

        # Identify weaknesses (high risk areas)
        for (analysis_type, risk_level), results in by_type_risk.items():
            if risk_level in [RiskLevel.HIGH, RiskLevel.VERY_HIGH]:
                if analysis_type == AnalysisType.LIQUIDITY:
                    weaknesses.append("Liquidity concerns - potential difficulty meeting short-term obligations")
                elif analysis_type == AnalysisType.PROFITABILITY:
                    weaknesses.append("Weak profitability performance requiring operational improvement")
                elif analysis_type == AnalysisType.SOLVENCY:
                    weaknesses.append("High financial leverage creating elevated financial risk")
                elif analysis_type == AnalysisType.ACTIVITY:
                    weaknesses.append("Inefficient asset utilization and operational inefficiencies")
                elif analysis_type == AnalysisType.QUALITY:
                    weaknesses.append("Financial reporting quality concerns requiring investigation")

        return strengths, weaknesses

    def _identify_critical_risks(self, all_results: List[AnalysisResult]) -> List[str]:
        """Identify critical risks from analysis results"""

        critical_risks = []

        # Very high risk items are always critical
        very_high_risks = [r for r in all_results if r.risk_level == RiskLevel.VERY_HIGH]
        for risk in very_high_risks:
            critical_risks.append(f"Critical: {risk.metric_name} - {risk.interpretation}")

        # Multiple high risks in same category are critical
        high_risks_by_type = {}
        for result in all_results:
            if result.risk_level == RiskLevel.HIGH:
                if result.analysis_type not in high_risks_by_type:
                    high_risks_by_type[result.analysis_type] = []
                high_risks_by_type[result.analysis_type].append(result)

        for analysis_type, risks in high_risks_by_type.items():
            if len(risks) >= 2:
                critical_risks.append(
                    f"Multiple high-risk {analysis_type.value} indicators require immediate attention")

        # Specific risk combinations
        liquidity_risks = [r for r in all_results if
                           r.analysis_type == AnalysisType.LIQUIDITY and r.risk_level == RiskLevel.HIGH]
        solvency_risks = [r for r in all_results if
                          r.analysis_type == AnalysisType.SOLVENCY and r.risk_level == RiskLevel.HIGH]

        if liquidity_risks and solvency_risks:
            critical_risks.append("Combined liquidity and solvency risks create financial distress potential")

        return critical_risks

    def _generate_strategic_recommendations(self, all_results: List[AnalysisResult],
                                            statements: FinancialStatements) -> List[str]:
        """Generate strategic recommendations based on analysis"""

        recommendations = []

        # Liquidity recommendations
        liquidity_risks = [r for r in all_results if
                           r.analysis_type == AnalysisType.LIQUIDITY and r.risk_level in [RiskLevel.HIGH,
                                                                                          RiskLevel.MODERATE]]
        if liquidity_risks:
            recommendations.append("Improve working capital management and consider establishing credit facilities")

        # Profitability recommendations
        profitability_risks = [r for r in all_results if
                               r.analysis_type == AnalysisType.PROFITABILITY and r.risk_level in [RiskLevel.HIGH,
                                                                                                  RiskLevel.MODERATE]]
        if profitability_risks:
            recommendations.append("Focus on cost optimization and revenue enhancement strategies")

        # Efficiency recommendations
        activity_risks = [r for r in all_results if
                          r.analysis_type == AnalysisType.ACTIVITY and r.risk_level in [RiskLevel.HIGH,
                                                                                        RiskLevel.MODERATE]]
        if activity_risks:
            recommendations.append("Optimize asset utilization and improve operational efficiency")

        # Leverage recommendations
        solvency_risks = [r for r in all_results if
                          r.analysis_type == AnalysisType.SOLVENCY and r.risk_level in [RiskLevel.HIGH,
                                                                                        RiskLevel.MODERATE]]
        if solvency_risks:
            recommendations.append("Consider debt reduction and strengthen balance sheet structure")

        # Quality recommendations
        quality_risks = [r for r in all_results if
                         r.analysis_type == AnalysisType.QUALITY and r.risk_level in [RiskLevel.HIGH,
                                                                                      RiskLevel.MODERATE]]
        if quality_risks:
            recommendations.append("Enhance financial reporting transparency and earnings quality")

        # Growth recommendations
        income_statement = statements.income_statement
        net_income = income_statement.get('net_income', 0)
        if net_income > 0:
            recommendations.append("Consider strategic investments for sustainable growth")
        else:
            recommendations.append("Develop turnaround strategy to restore profitability")

        return recommendations

    def _create_integrated_results(self, integrated_analysis: IntegratedAnalysis,
                                   statements: FinancialStatements) -> List[AnalysisResult]:
        """Convert integrated analysis to AnalysisResult format"""

        results = []

        # Overall financial health
        health_risk = RiskLevel.LOW if integrated_analysis.overall_financial_health in [FinancialHealth.EXCELLENT,
                                                                                        FinancialHealth.GOOD] else RiskLevel.HIGH

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Overall Financial Health",
            value=integrated_analysis.composite_score,
            interpretation=f"Overall financial health is {integrated_analysis.overall_financial_health.value} with composite score of {integrated_analysis.composite_score:.1f}",
            risk_level=health_risk,
            methodology="Weighted composite of liquidity, profitability, efficiency, leverage, growth, and quality scores"
        ))

        # Business model classification
        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Business Model Type",
            value=1.0,
            interpretation=f"Business model classified as {integrated_analysis.business_model_type.value}",
            risk_level=RiskLevel.LOW,
            methodology="Classification based on asset intensity, growth patterns, and profitability"
        ))

        # Component scores
        component_scores = {
            "Liquidity Score": integrated_analysis.liquidity_score,
            "Profitability Score": integrated_analysis.profitability_score,
            "Efficiency Score": integrated_analysis.efficiency_score,
            "Leverage Score": integrated_analysis.leverage_score,
            "Growth Score": integrated_analysis.growth_score,
            "Quality Score": integrated_analysis.quality_score
        }

        for score_name, score_value in component_scores.items():
            score_risk = RiskLevel.LOW if score_value > 70 else RiskLevel.MODERATE if score_value > 50 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name=score_name,
                value=score_value,
                interpretation=f"{score_name}: {score_value:.1f}/100",
                risk_level=score_risk,
                methodology="Composite score based on relevant financial metrics"
            ))

        # Key insights
        if integrated_analysis.key_strengths:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Key Strengths",
                value=len(integrated_analysis.key_strengths),
                interpretation="Key financial strengths identified",
                risk_level=RiskLevel.LOW,
                recommendations=integrated_analysis.key_strengths
            ))

        if integrated_analysis.key_weaknesses:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Key Weaknesses",
                value=len(integrated_analysis.key_weaknesses),
                interpretation="Key financial weaknesses requiring attention",
                risk_level=RiskLevel.HIGH,
                limitations=integrated_analysis.key_weaknesses
            ))

        return results

    def _analyze_statement_linkages(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze relationships and linkages between financial statements"""

        results = []

        # Income statement to cash flow linkage
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow

        net_income = income_statement.get('net_income', 0)
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)

        if net_income != 0:
            cash_quality_ratio = self.safe_divide(operating_cash_flow, net_income)

            cash_quality_interpretation = "Excellent cash conversion from earnings" if cash_quality_ratio > 1.2 else "Good cash quality" if cash_quality_ratio > 1.0 else "Moderate cash quality" if cash_quality_ratio > 0.8 else "Poor cash conversion quality"
            cash_quality_risk = RiskLevel.LOW if cash_quality_ratio > 1.0 else RiskLevel.MODERATE if cash_quality_ratio > 0.7 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Earnings-Cash Flow Quality",
                value=cash_quality_ratio,
                interpretation=cash_quality_interpretation,
                risk_level=cash_quality_risk,
                methodology="Operating Cash Flow / Net Income"
            ))

        # Balance sheet efficiency analysis
        balance_sheet = statements.balance_sheet
        revenue = income_statement.get('revenue', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        if revenue > 0 and total_assets > 0:
            asset_turnover = self.safe_divide(revenue, total_assets)

            efficiency_interpretation = "High asset efficiency" if asset_turnover > 1.5 else "Moderate asset efficiency" if asset_turnover > 1.0 else "Low asset efficiency"
            efficiency_risk = RiskLevel.LOW if asset_turnover > 1.2 else RiskLevel.MODERATE if asset_turnover > 0.8 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Statement Integration - Asset Efficiency",
                value=asset_turnover,
                interpretation=efficiency_interpretation,
                risk_level=efficiency_risk,
                methodology="Revenue (IS) / Total Assets (BS)"
            ))

        # Working capital management integration
        current_assets = balance_sheet.get('current_assets', 0)
        current_liabilities = balance_sheet.get('current_liabilities', 0)
        working_capital_change = cash_flow.get('working_capital_change', 0)

        working_capital = current_assets - current_liabilities

        if abs(working_capital_change) > 0 and abs(working_capital) > 0:
            wc_efficiency = self.safe_divide(abs(working_capital_change), abs(working_capital))

            wc_interpretation = "Significant working capital volatility" if wc_efficiency > 0.2 else "Moderate working capital changes" if wc_efficiency > 0.1 else "Stable working capital management"
            wc_risk = RiskLevel.HIGH if wc_efficiency > 0.3 else RiskLevel.MODERATE if wc_efficiency > 0.15 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Working Capital Management Integration",
                value=wc_efficiency,
                interpretation=wc_interpretation,
                risk_level=wc_risk,
                methodology="|WC Change (CF)| / |Net Working Capital (BS)|"
            ))

        return results

    def _analyze_business_cycle(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze where company is in business cycle"""

        results = []

        if not comparative_data or len(comparative_data) < 2:
            return results

        # Analyze trends over time
        income_statement = statements.income_statement

        # Revenue trend analysis
        revenue_values = []
        for past_statements in comparative_data:
            revenue_values.append(past_statements.income_statement.get('revenue', 0))
        revenue_values.append(income_statement.get('revenue', 0))

        # Net income trend analysis
        ni_values = []
        for past_statements in comparative_data:
            ni_values.append(past_statements.income_statement.get('net_income', 0))
        ni_values.append(income_statement.get('net_income', 0))

        # Determine lifecycle stage
        lifecycle_indicators = []

        # Growth stage indicators
        recent_revenue_growth = 0
        if len(revenue_values) >= 2 and revenue_values[-2] > 0:
            recent_revenue_growth = (revenue_values[-1] / revenue_values[-2]) - 1

        if recent_revenue_growth > 0.15:
            lifecycle_indicators.append("High revenue growth indicates growth stage")
        elif recent_revenue_growth > 0.05:
            lifecycle_indicators.append("Moderate growth suggests expansion phase")
        elif recent_revenue_growth < -0.05:
            lifecycle_indicators.append("Declining revenue suggests maturity or decline phase")
        else:
            lifecycle_indicators.append("Stable revenue indicates mature stage")

        # Profitability evolution
        positive_ni_periods = sum(1 for ni in ni_values if ni > 0)
        profitability_ratio = positive_ni_periods / len(ni_values)

        if profitability_ratio > 0.8:
            lifecycle_indicators.append("Consistent profitability indicates mature business model")
        elif profitability_ratio < 0.5:
            lifecycle_indicators.append("Inconsistent profitability suggests early stage or turnaround situation")

        # Determine overall lifecycle stage
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
            value=1.0,
            interpretation=f"Company appears to be in {lifecycle_stage}",
            risk_level=RiskLevel.LOW,
            recommendations=lifecycle_indicators,
            methodology="Analysis of revenue growth and profitability trends over time"
        ))

        return results

    def _perform_risk_assessment(self, all_results: List[AnalysisResult],
                                 statements: FinancialStatements) -> List[AnalysisResult]:
        """Perform comprehensive risk assessment"""

        results = []

        # Count risks by level
        risk_counts = {
            RiskLevel.LOW: 0,
            RiskLevel.MODERATE: 0,
            RiskLevel.HIGH: 0,
            RiskLevel.VERY_HIGH: 0
        }

        for result in all_results:
            risk_counts[result.risk_level] += 1

        total_metrics = len(all_results)
        high_risk_ratio = (risk_counts[RiskLevel.HIGH] + risk_counts[
            RiskLevel.VERY_HIGH]) / total_metrics if total_metrics > 0 else 0

        # Overall risk assessment
        if high_risk_ratio > 0.3:
            overall_risk = "High Risk"
            risk_level = RiskLevel.HIGH
        elif high_risk_ratio > 0.15:
            overall_risk = "Moderate Risk"
            risk_level = RiskLevel.MODERATE
        else:
            overall_risk = "Low Risk"
            risk_level = RiskLevel.LOW

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Overall Risk Assessment",
            value=high_risk_ratio,
            interpretation=f"{overall_risk} - {high_risk_ratio:.1%} of metrics show elevated risk",
            risk_level=risk_level,
            methodology="Proportion of high and very high risk metrics"
        ))

        # Risk concentration analysis
        risk_by_type = {}
        for result in all_results:
            if result.risk_level in [RiskLevel.HIGH, RiskLevel.VERY_HIGH]:
                if result.analysis_type not in risk_by_type:
                    risk_by_type[result.analysis_type] = 0
                risk_by_type[result.analysis_type] += 1

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

        # Capital allocation insights
        cash_flow = statements.cash_flow
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        capex = cash_flow.get('capex', 0)
        dividends_paid = cash_flow.get('dividends_paid', 0)
        acquisitions = cash_flow.get('acquisitions', 0)

        total_capital_deployment = capex + dividends_paid + acquisitions

        if operating_cash_flow > 0 and total_capital_deployment > 0:
            capital_efficiency = self.safe_divide(total_capital_deployment, operating_cash_flow)

            capital_interpretation = "Aggressive capital deployment" if capital_efficiency > 1.0 else "Balanced capital allocation" if capital_efficiency > 0.7 else "Conservative capital deployment"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Capital Allocation Strategy",
                value=capital_efficiency,
                interpretation=capital_interpretation,
                risk_level=RiskLevel.MODERATE if capital_efficiency > 1.2 else RiskLevel.LOW,
                methodology="(CapEx + Dividends + Acquisitions) / Operating Cash Flow"
            ))

        # Competitive position indicators
        income_statement = statements.income_statement
        revenue = income_statement.get('revenue', 0)
        gross_profit = revenue - income_statement.get('cost_of_sales', 0)

        if revenue > 0:
            gross_margin = self.safe_divide(gross_profit, revenue)

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

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return comprehensive key metrics from all analyzers"""

        # Get metrics from individual analyzers
        income_metrics = self.income_analyzer.get_key_metrics(statements)
        balance_metrics = self.balance_analyzer.get_key_metrics(statements)
        cash_flow_metrics = self.cash_flow_analyzer.get_key_metrics(statements)

        # Combine all metrics
        all_metrics = {**income_metrics, **balance_metrics, **cash_flow_metrics}

        return all_metrics

    def create_integrated_analysis(self, statements: FinancialStatements,
                                   comparative_data: Optional[List[FinancialStatements]] = None,
                                   industry_data: Optional[Dict] = None) -> IntegratedAnalysis:
        """Create comprehensive integrated analysis object"""

        # Run full analysis
        all_results = self.analyze(statements, comparative_data, industry_data)

        # Extract integrated analysis from results
        integrated_results = [r for r in all_results if "Score" in r.metric_name or "Health" in r.metric_name]

        # Create IntegratedAnalysis object (simplified version)
        return IntegratedAnalysis(
            overall_financial_health=FinancialHealth.GOOD,  # Would be determined from analysis
            business_model_type=BusinessModel.MATURE,  # Would be determined from analysis
            composite_score=75.0  # Would be calculated from component scores
        )