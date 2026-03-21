
"""
Financial Statement Employee Compensation Module
========================================

Employee compensation analysis and impact assessment

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

# Import from core modules
from ..core.base_analyzer import BaseAnalyzer, AnalysisResult, AnalysisType, RiskLevel, TrendDirection, \
    ComparativeAnalysis
from ..core.data_processor import FinancialStatements, ReportingStandard


class CompensationType(Enum):
    """Types of employee compensation"""
    CASH_WAGES = "cash_wages_salaries"
    POST_EMPLOYMENT = "post_employment_benefits"
    SHARE_BASED = "share_based_compensation"
    OTHER_BENEFITS = "other_employee_benefits"


class PensionPlanType(Enum):
    """Types of pension plans"""
    DEFINED_CONTRIBUTION = "defined_contribution"
    DEFINED_BENEFIT = "defined_benefit"
    HYBRID = "hybrid_plan"


class ShareBasedType(Enum):
    """Types of share-based compensation"""
    STOCK_OPTIONS = "stock_options"
    RESTRICTED_STOCK = "restricted_stock"
    PERFORMANCE_SHARES = "performance_shares"
    STOCK_APPRECIATION_RIGHTS = "stock_appreciation_rights"
    EMPLOYEE_STOCK_PURCHASE = "employee_stock_purchase_plan"


class FundingStatus(Enum):
    """Pension plan funding status"""
    OVERFUNDED = "overfunded"
    FULLY_FUNDED = "fully_funded"
    UNDERFUNDED = "underfunded"
    SEVERELY_UNDERFUNDED = "severely_underfunded"


@dataclass
class PostEmploymentAnalysis:
    """Post-employment benefits analysis"""
    total_pension_obligation: float
    plan_assets_fair_value: float
    funded_status: float
    funding_ratio: float

    # Plan breakdown
    defined_benefit_obligation: float
    defined_contribution_assets: float

    # Annual costs
    service_cost: float
    interest_cost: float
    expected_return_on_assets: float
    net_periodic_cost: float

    # Risk factors
    funding_status_enum: FundingStatus
    actuarial_assumptions_risk: RiskLevel
    demographic_risk: RiskLevel
    investment_risk: RiskLevel


@dataclass
class ShareBasedCompensationAnalysis:
    """Share-based compensation analysis"""
    total_sbc_expense: float
    sbc_intensity: float  # SBC / Revenue

    # Composition
    stock_option_expense: float
    restricted_stock_expense: float
    performance_share_expense: float

    # Dilution metrics
    potential_dilution: float
    weighted_average_dilutive_shares: float

    # Valuation metrics
    fair_value_assumptions: Dict[str, float] = field(default_factory=dict)
    expense_timing_pattern: str = ""

    # Strategic implications
    retention_effectiveness: str = ""
    performance_alignment: RiskLevel = RiskLevel.MODERATE


@dataclass
class CompensationStrategy:
    """Overall compensation strategy analysis"""
    total_compensation_expense: float
    compensation_intensity: float  # Total comp / Revenue

    # Mix analysis
    cash_compensation_ratio: float
    equity_compensation_ratio: float
    benefits_ratio: float

    # Benchmarking
    industry_competitiveness: str = ""
    retention_risk: RiskLevel = RiskLevel.MODERATE
    cost_efficiency: float = 0.0


class EmployeeCompensationAnalyzer(BaseAnalyzer):
    """
    Comprehensive employee compensation analyzer implementing CFA Level II standards.
    Covers post-employment benefits and share-based compensation.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_compensation_formulas()
        self._initialize_compensation_benchmarks()

    def _initialize_compensation_formulas(self):
        """Initialize compensation-specific formulas"""
        self.formula_registry.update({
            'funding_ratio': lambda plan_assets, pension_obligation: self.safe_divide(plan_assets, pension_obligation),
            'sbc_intensity': lambda sbc_expense, revenue: self.safe_divide(sbc_expense, revenue),
            'compensation_intensity': lambda total_comp, revenue: self.safe_divide(total_comp, revenue),
            'dilution_impact': lambda dilutive_shares, basic_shares: self.safe_divide(dilutive_shares, basic_shares),
            'pension_cost_ratio': lambda pension_cost, operating_income: self.safe_divide(pension_cost,
                                                                                          operating_income),
            'benefit_coverage': lambda plan_assets, current_liabilities: self.safe_divide(plan_assets,
                                                                                          current_liabilities)
        })

    def _initialize_compensation_benchmarks(self):
        """Initialize compensation-specific benchmarks"""
        self.compensation_benchmarks = {
            'funding_ratio': {'overfunded': 1.1, 'fully_funded': 1.0, 'underfunded': 0.9, 'severely_underfunded': 0.8},
            'sbc_intensity': {'low': 0.02, 'moderate': 0.05, 'high': 0.10, 'very_high': 0.20},
            'compensation_intensity': {'low': 0.3, 'moderate': 0.4, 'high': 0.6, 'very_high': 0.8},
            'dilution_impact': {'minimal': 0.02, 'moderate': 0.05, 'significant': 0.10, 'excessive': 0.20}
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive employee compensation analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all compensation aspects
        """
        results = []

        # Post-employment benefits analysis
        results.extend(self._analyze_post_employment_benefits(statements, comparative_data, industry_data))

        # Share-based compensation analysis
        results.extend(self._analyze_share_based_compensation(statements, comparative_data, industry_data))

        # Overall compensation strategy
        results.extend(self._analyze_compensation_strategy(statements, comparative_data, industry_data))

        # Pension risk assessment
        results.extend(self._assess_pension_risks(statements, comparative_data))

        # SBC forecasting implications
        results.extend(self._analyze_sbc_forecasting(statements, comparative_data))

        # Valuation considerations
        results.extend(self._assess_valuation_impact(statements, comparative_data))

        return results

    def _analyze_post_employment_benefits(self, statements: FinancialStatements,
                                          comparative_data: Optional[List[FinancialStatements]] = None,
                                          industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze post-employment benefit plans"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        # Extract pension-related data
        pension_obligation = balance_sheet.get('pension_obligation', 0)
        pension_assets = balance_sheet.get('pension_plan_assets', 0)
        pension_liability = balance_sheet.get('pension_liability', 0)

        pension_expense = income_statement.get('pension_expense', 0)
        service_cost = notes.get('pension_service_cost', 0)
        interest_cost = notes.get('pension_interest_cost', 0)
        expected_return = notes.get('expected_return_plan_assets', 0)

        if pension_obligation <= 0 and pension_expense <= 0:
            return results

        # Funding Status Analysis
        if pension_obligation > 0:
            funded_status = pension_assets - pension_obligation
            funding_ratio = self.safe_divide(pension_assets, pension_obligation)

            # Determine funding status category
            if funding_ratio >= 1.1:
                funding_status = FundingStatus.OVERFUNDED
                funding_interpretation = "Pension plan is overfunded - surplus available"
                funding_risk = RiskLevel.LOW
            elif funding_ratio >= 1.0:
                funding_status = FundingStatus.FULLY_FUNDED
                funding_interpretation = "Pension plan is fully funded"
                funding_risk = RiskLevel.LOW
            elif funding_ratio >= 0.8:
                funding_status = FundingStatus.UNDERFUNDED
                funding_interpretation = "Pension plan is underfunded - future contributions required"
                funding_risk = RiskLevel.MODERATE
            else:
                funding_status = FundingStatus.SEVERELY_UNDERFUNDED
                funding_interpretation = "Pension plan is severely underfunded - significant funding risk"
                funding_risk = RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Pension Funding Ratio",
                value=funding_ratio,
                interpretation=funding_interpretation,
                risk_level=funding_risk,
                benchmark_comparison=self.compare_to_industry(funding_ratio, industry_data.get(
                    'pension_funding_ratio') if industry_data else None),
                methodology="Plan Assets / Pension Benefit Obligation",
                limitations=["Funding ratio based on actuarial assumptions that may change"]
            ))

            # Funded Status Impact on Balance Sheet
            total_assets = balance_sheet.get('total_assets', 0)
            if total_assets > 0:
                funded_status_ratio = self.safe_divide(abs(funded_status), total_assets)

                if funded_status < 0:  # Underfunded
                    status_interpretation = f"Pension underfunding represents {self.format_percentage(funded_status_ratio)} of total assets"
                    status_risk = RiskLevel.HIGH if funded_status_ratio > 0.1 else RiskLevel.MODERATE if funded_status_ratio > 0.05 else RiskLevel.LOW
                else:  # Overfunded
                    status_interpretation = f"Pension overfunding represents {self.format_percentage(funded_status_ratio)} of total assets"
                    status_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.SOLVENCY,
                    metric_name="Pension Funded Status Impact",
                    value=funded_status_ratio,
                    interpretation=status_interpretation,
                    risk_level=status_risk,
                    methodology="|Funded Status| / Total Assets"
                ))

        # Pension Cost Analysis
        if pension_expense > 0:
            revenue = income_statement.get('revenue', 0)
            if revenue > 0:
                pension_cost_intensity = self.safe_divide(pension_expense, revenue)

                cost_interpretation = "High pension cost burden" if pension_cost_intensity > 0.05 else "Moderate pension costs" if pension_cost_intensity > 0.02 else "Low pension cost impact"
                cost_risk = RiskLevel.MODERATE if pension_cost_intensity > 0.08 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.PROFITABILITY,
                    metric_name="Pension Cost Intensity",
                    value=pension_cost_intensity,
                    interpretation=cost_interpretation,
                    risk_level=cost_risk,
                    methodology="Pension Expense / Revenue"
                ))

        # Service Cost vs Interest Cost Analysis
        if service_cost > 0 and interest_cost > 0:
            total_cost_components = service_cost + interest_cost
            service_cost_ratio = self.safe_divide(service_cost, total_cost_components)

            service_interpretation = "Service cost dominates - active workforce driving costs" if service_cost_ratio > 0.6 else "Balanced service and interest costs" if service_cost_ratio > 0.4 else "Interest cost dominates - mature plan with large obligation"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Pension Cost Composition",
                value=service_cost_ratio,
                interpretation=service_interpretation,
                risk_level=RiskLevel.LOW,
                methodology="Service Cost / (Service Cost + Interest Cost)"
            ))

        # Expected Return vs Actual Return Analysis
        actual_return = notes.get('actual_return_plan_assets', 0)
        if expected_return > 0 and actual_return != 0:
            return_variance = actual_return - expected_return
            return_variance_ratio = self.safe_divide(abs(return_variance), abs(expected_return))

            variance_interpretation = "Significant variance between expected and actual returns" if return_variance_ratio > 0.2 else "Moderate return variance" if return_variance_ratio > 0.1 else "Returns close to expectations"
            variance_risk = RiskLevel.MODERATE if return_variance_ratio > 0.3 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Pension Return Variance",
                value=return_variance_ratio,
                interpretation=variance_interpretation,
                risk_level=variance_risk,
                methodology="|Actual Return - Expected Return| / |Expected Return|"
            ))

        return results

    def _analyze_share_based_compensation(self, statements: FinancialStatements,
                                          comparative_data: Optional[List[FinancialStatements]] = None,
                                          industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze share-based compensation"""
        results = []

        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet
        notes = statements.notes

        sbc_expense = income_statement.get('stock_compensation', 0)
        revenue = income_statement.get('revenue', 0)

        if sbc_expense <= 0:
            return results

        # SBC Intensity Analysis
        if revenue > 0:
            sbc_intensity = self.safe_divide(sbc_expense, revenue)
            benchmark = self.compensation_benchmarks['sbc_intensity']

            if sbc_intensity > benchmark['very_high']:
                intensity_interpretation = "Very high share-based compensation intensity - significant equity dilution concern"
                intensity_risk = RiskLevel.HIGH
            elif sbc_intensity > benchmark['high']:
                intensity_interpretation = "High share-based compensation usage"
                intensity_risk = RiskLevel.MODERATE
            elif sbc_intensity > benchmark['moderate']:
                intensity_interpretation = "Moderate share-based compensation"
                intensity_risk = RiskLevel.LOW
            else:
                intensity_interpretation = "Low share-based compensation usage"
                intensity_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Share-Based Compensation Intensity",
                value=sbc_intensity,
                interpretation=intensity_interpretation,
                risk_level=intensity_risk,
                benchmark_comparison=self.compare_to_industry(sbc_intensity, industry_data.get(
                    'sbc_intensity') if industry_data else None),
                methodology="Stock-Based Compensation Expense / Revenue",
                limitations=["High SBC may indicate cash conservation or growth stage"]
            ))

        # Dilution Impact Analysis
        basic_shares = income_statement.get('shares_outstanding_basic', 0)
        diluted_shares = income_statement.get('shares_outstanding_diluted', 0)

        if basic_shares > 0 and diluted_shares > basic_shares:
            dilutive_shares = diluted_shares - basic_shares
            dilution_impact = self.safe_divide(dilutive_shares, basic_shares)
            benchmark = self.compensation_benchmarks['dilution_impact']

            if dilution_impact > benchmark['excessive']:
                dilution_interpretation = "Excessive dilution from share-based compensation"
                dilution_risk = RiskLevel.HIGH
            elif dilution_impact > benchmark['significant']:
                dilution_interpretation = "Significant dilution impact"
                dilution_risk = RiskLevel.MODERATE
            elif dilution_impact > benchmark['moderate']:
                dilution_interpretation = "Moderate dilution from SBC"
                dilution_risk = RiskLevel.LOW
            else:
                dilution_interpretation = "Minimal dilution impact"
                dilution_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="SBC Dilution Impact",
                value=dilution_impact,
                interpretation=dilution_interpretation,
                risk_level=dilution_risk,
                methodology="(Diluted Shares - Basic Shares) / Basic Shares"
            ))

        # SBC Expense Trend Analysis
        if comparative_data and len(comparative_data) >= 2:
            sbc_values = []
            revenue_values = []

            for past_statements in comparative_data:
                past_sbc = past_statements.income_statement.get('stock_compensation', 0)
                past_revenue = past_statements.income_statement.get('revenue', 0)
                sbc_values.append(past_sbc)
                revenue_values.append(past_revenue)

            sbc_values.append(sbc_expense)
            revenue_values.append(revenue)

            if len(sbc_values) > 2:
                sbc_trend = self.calculate_trend(sbc_values, [f"Period-{i}" for i in range(len(sbc_values))])

                # Compare SBC growth to revenue growth
                if len(revenue_values) == len(sbc_values):
                    revenue_trend = self.calculate_trend(revenue_values,
                                                         [f"Period-{i}" for i in range(len(revenue_values))])

                    if sbc_trend.growth_rate and revenue_trend.growth_rate:
                        relative_growth = sbc_trend.growth_rate - revenue_trend.growth_rate

                        if relative_growth > 0.1:
                            trend_interpretation = "SBC expense growing faster than revenue - increasing compensation intensity"
                            trend_risk = RiskLevel.MODERATE
                        elif relative_growth > -0.1:
                            trend_interpretation = "SBC expense growth aligned with revenue growth"
                            trend_risk = RiskLevel.LOW
                        else:
                            trend_interpretation = "SBC expense declining relative to revenue - improving efficiency"
                            trend_risk = RiskLevel.LOW

                        results.append(AnalysisResult(
                            analysis_type=AnalysisType.QUALITY,
                            metric_name="SBC Growth vs Revenue Growth",
                            value=relative_growth,
                            interpretation=trend_interpretation,
                            risk_level=trend_risk,
                            methodology="SBC Growth Rate - Revenue Growth Rate"
                        ))

        # SBC Composition Analysis
        stock_option_expense = notes.get('stock_option_expense', 0)
        restricted_stock_expense = notes.get('restricted_stock_expense', 0)
        performance_share_expense = notes.get('performance_share_expense', 0)

        total_detailed_sbc = stock_option_expense + restricted_stock_expense + performance_share_expense

        if total_detailed_sbc > 0 and abs(total_detailed_sbc - sbc_expense) / sbc_expense < 0.1:
            # Analyze composition if detailed breakdown is available
            sbc_types = {
                'Stock Options': stock_option_expense,
                'Restricted Stock': restricted_stock_expense,
                'Performance Shares': performance_share_expense
            }

            for sbc_type, sbc_value in sbc_types.items():
                if sbc_value > 0:
                    sbc_type_ratio = self.safe_divide(sbc_value, total_detailed_sbc)

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name=f"{sbc_type} Composition",
                        value=sbc_type_ratio,
                        interpretation=f"{sbc_type} represents {self.format_percentage(sbc_type_ratio)} of total SBC expense",
                        risk_level=RiskLevel.LOW,
                        methodology=f"{sbc_type} Expense / Total SBC Expense"
                    ))

        # Performance-Based Compensation Analysis
        if performance_share_expense > 0:
            performance_ratio = self.safe_divide(performance_share_expense, sbc_expense)

            performance_interpretation = "High performance-based compensation alignment" if performance_ratio > 0.4 else "Moderate performance alignment" if performance_ratio > 0.2 else "Limited performance-based compensation"
            performance_risk = RiskLevel.LOW if performance_ratio > 0.3 else RiskLevel.MODERATE

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Performance-Based SBC Ratio",
                value=performance_ratio,
                interpretation=performance_interpretation,
                risk_level=performance_risk,
                methodology="Performance Share Expense / Total SBC Expense",
                limitations=["Performance alignment depends on specific performance metrics used"]
            ))

        return results

    def _analyze_compensation_strategy(self, statements: FinancialStatements,
                                       comparative_data: Optional[List[FinancialStatements]] = None,
                                       industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze overall compensation strategy"""
        results = []

        income_statement = statements.income_statement

        # Total compensation components
        employee_costs = income_statement.get('employee_costs', 0)
        pension_expense = income_statement.get('pension_expense', 0)
        sbc_expense = income_statement.get('stock_compensation', 0)
        other_benefits = income_statement.get('other_employee_benefits', 0)

        total_compensation = employee_costs + pension_expense + sbc_expense + other_benefits
        revenue = income_statement.get('revenue', 0)

        if total_compensation <= 0:
            return results

        # Total Compensation Intensity
        if revenue > 0:
            compensation_intensity = self.safe_divide(total_compensation, revenue)
            benchmark = self.compensation_benchmarks['compensation_intensity']

            if compensation_intensity > benchmark['very_high']:
                intensity_interpretation = "Very high compensation intensity - labor-intensive business model"
                intensity_risk = RiskLevel.MODERATE
            elif compensation_intensity > benchmark['high']:
                intensity_interpretation = "High compensation costs relative to revenue"
                intensity_risk = RiskLevel.MODERATE
            elif compensation_intensity > benchmark['moderate']:
                intensity_interpretation = "Moderate compensation intensity"
                intensity_risk = RiskLevel.LOW
            else:
                intensity_interpretation = "Low compensation intensity - capital-intensive or automated business"
                intensity_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Total Compensation Intensity",
                value=compensation_intensity,
                interpretation=intensity_interpretation,
                risk_level=intensity_risk,
                benchmark_comparison=self.compare_to_industry(compensation_intensity, industry_data.get(
                    'compensation_intensity') if industry_data else None),
                methodology="Total Employee Compensation / Revenue"
            ))

        # Compensation Mix Analysis
        if total_compensation > 0:
            cash_compensation_ratio = self.safe_divide(employee_costs, total_compensation)
            equity_compensation_ratio = self.safe_divide(sbc_expense, total_compensation)
            benefits_ratio = self.safe_divide(pension_expense + other_benefits, total_compensation)

            mix_components = {
                'Cash Compensation Ratio': cash_compensation_ratio,
                'Equity Compensation Ratio': equity_compensation_ratio,
                'Benefits Ratio': benefits_ratio
            }

            for component, ratio in mix_components.items():
                if ratio > 0:
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name=component,
                        value=ratio,
                        interpretation=f"{component.replace('_', ' ')} of {self.format_percentage(ratio)}",
                        risk_level=RiskLevel.LOW,
                        methodology=f"Component / Total Compensation"
                    ))

            # Strategic assessment of mix
            if equity_compensation_ratio > 0.2:
                strategy_assessment = "Equity-heavy compensation strategy - retention and performance focus"
                strategy_risk = RiskLevel.MODERATE
            elif benefits_ratio > 0.3:
                strategy_assessment = "Benefits-heavy compensation - traditional employment model"
                strategy_risk = RiskLevel.LOW
            else:
                strategy_assessment = "Cash-focused compensation strategy"
                strategy_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Compensation Strategy Assessment",
                value=1.0,
                interpretation=strategy_assessment,
                risk_level=strategy_risk,
                methodology="Qualitative assessment of compensation mix"
            ))

        return results

    def _assess_pension_risks(self, statements: FinancialStatements,
                              comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Assess pension-related risks"""
        results = []

        notes = statements.notes

        # Actuarial Assumption Risk
        discount_rate = notes.get('pension_discount_rate', 0)
        expected_return_rate = notes.get('expected_return_rate', 0)
        salary_increase_rate = notes.get('salary_increase_assumption', 0)

        if discount_rate > 0:
            # Assess discount rate appropriateness (simplified)
            if discount_rate < 0.03:
                discount_interpretation = "Very low discount rate increases pension obligation sensitivity"
                discount_risk = RiskLevel.HIGH
            elif discount_rate < 0.05:
                discount_interpretation = "Low discount rate environment - moderate sensitivity"
                discount_risk = RiskLevel.MODERATE
            else:
                discount_interpretation = "Reasonable discount rate assumption"
                discount_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Pension Discount Rate Risk",
                value=discount_rate,
                interpretation=discount_interpretation,
                risk_level=discount_risk,
                methodology="Assessment of discount rate level and sensitivity",
                limitations=["Discount rate changes significantly impact pension obligations"]
            ))

        # Expected Return vs Discount Rate Analysis
        if expected_return_rate > 0 and discount_rate > 0:
            return_premium = expected_return_rate - discount_rate

            if return_premium > 0.02:
                return_interpretation = "High expected return premium - aggressive investment assumption"
                return_risk = RiskLevel.MODERATE
            elif return_premium > 0:
                return_interpretation = "Positive expected return premium"
                return_risk = RiskLevel.LOW
            else:
                return_interpretation = "Conservative expected return assumption"
                return_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Expected Return Premium",
                value=return_premium,
                interpretation=return_interpretation,
                risk_level=return_risk,
                methodology="Expected Return Rate - Discount Rate"
            ))

        # Demographic Risk Assessment
        average_participant_age = notes.get('average_participant_age', 0)
        if average_participant_age > 0:
            if average_participant_age > 55:
                demographic_interpretation = "Aging participant base - increasing near-term benefit payments"
                demographic_risk = RiskLevel.MODERATE
            elif average_participant_age > 45:
                demographic_interpretation = "Mature participant base"
                demographic_risk = RiskLevel.LOW
            else:
                demographic_interpretation = "Young participant base - deferred benefit payments"
                demographic_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Pension Demographic Risk",
                value=average_participant_age,
                interpretation=demographic_interpretation,
                risk_level=demographic_risk,
                methodology="Assessment of participant age profile"
            ))

        return results

    def _analyze_sbc_forecasting(self, statements: FinancialStatements,
                                 comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze SBC forecasting implications"""
        results = []

        notes = statements.notes
        income_statement = statements.income_statement

        # Unvested SBC Analysis
        unvested_sbc_value = notes.get('unvested_sbc_value', 0)
        weighted_average_vesting_period = notes.get('weighted_average_vesting_period', 0)

        if unvested_sbc_value > 0:
            # Future expense estimation
            current_sbc_expense = income_statement.get('stock_compensation', 0)

            if weighted_average_vesting_period > 0:
                estimated_annual_expense = self.safe_divide(unvested_sbc_value, weighted_average_vesting_period)

                if current_sbc_expense > 0:
                    future_expense_ratio = self.safe_divide(estimated_annual_expense, current_sbc_expense)

                    if future_expense_ratio > 1.2:
                        forecasting_interpretation = "SBC expense expected to increase significantly based on unvested awards"
                        forecasting_risk = RiskLevel.MODERATE
                    elif future_expense_ratio > 0.8:
                        forecasting_interpretation = "SBC expense expected to remain stable"
                        forecasting_risk = RiskLevel.LOW
                    else:
                        forecasting_interpretation = "SBC expense expected to decline"
                        forecasting_risk = RiskLevel.LOW

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name="SBC Future Expense Indicator",
                        value=future_expense_ratio,
                        interpretation=forecasting_interpretation,
                        risk_level=forecasting_risk,
                        methodology="Estimated Future Annual SBC Expense / Current SBC Expense"
                    ))

        # Share Count Projections
        options_outstanding = notes.get('stock_options_outstanding', 0)
        weighted_average_exercise_price = notes.get('weighted_average_exercise_price', 0)
        current_stock_price = notes.get('current_stock_price', 0)

        if options_outstanding > 0 and current_stock_price > 0 and weighted_average_exercise_price > 0:
            # Estimate potential dilution from in-the-money options
            if current_stock_price > weighted_average_exercise_price:
                intrinsic_value_ratio = (current_stock_price - weighted_average_exercise_price) / current_stock_price

                if intrinsic_value_ratio > 0.3:
                    dilution_interpretation = "Significant in-the-money options - high exercise probability"
                    dilution_risk = RiskLevel.MODERATE
                elif intrinsic_value_ratio > 0.1:
                    dilution_interpretation = "Moderate in-the-money options"
                    dilution_risk = RiskLevel.LOW
                else:
                    dilution_interpretation = "Limited intrinsic value in outstanding options"
                    dilution_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Option Intrinsic Value Ratio",
                    value=intrinsic_value_ratio,
                    interpretation=dilution_interpretation,
                    risk_level=dilution_risk,
                    methodology="(Current Price - Exercise Price) / Current Price"
                ))

        return results

    def _assess_valuation_impact(self, statements: FinancialStatements,
                                 comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Assess valuation implications of compensation arrangements"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        # Pension Obligation Impact on Enterprise Value
        pension_obligation = balance_sheet.get('pension_obligation', 0)
        pension_assets = balance_sheet.get('pension_plan_assets', 0)
        net_pension_liability = pension_obligation - pension_assets

        if net_pension_liability > 0:
            total_debt = balance_sheet.get('long_term_debt', 0) + balance_sheet.get('short_term_debt', 0)

            if total_debt > 0:
                pension_debt_ratio = self.safe_divide(net_pension_liability, total_debt)

                valuation_interpretation = "Pension liability significantly impacts debt-like obligations" if pension_debt_ratio > 0.5 else "Moderate pension liability impact" if pension_debt_ratio > 0.2 else "Limited pension liability impact on valuation"
                valuation_risk = RiskLevel.MODERATE if pension_debt_ratio > 0.4 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.VALUATION,
                    metric_name="Pension Liability to Debt Ratio",
                    value=pension_debt_ratio,
                    interpretation=valuation_interpretation,
                    risk_level=valuation_risk,
                    methodology="Net Pension Liability / Total Debt",
                    limitations=["Pension obligations should be considered in enterprise valuation"]
                ))

        # SBC Cash Flow Impact
        sbc_expense = income_statement.get('stock_compensation', 0)
        tax_rate = 0.25  # Simplified assumption

        if sbc_expense > 0:
            # SBC provides tax deduction but no cash cost
            sbc_tax_benefit = sbc_expense * tax_rate

            cash_flow_benefit_ratio = self.safe_divide(sbc_tax_benefit, sbc_expense)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.VALUATION,
                metric_name="SBC Tax Benefit Ratio",
                value=cash_flow_benefit_ratio,
                interpretation=f"SBC provides tax benefits worth {self.format_percentage(cash_flow_benefit_ratio)} of expense",
                risk_level=RiskLevel.LOW,
                methodology="(SBC Expense × Tax Rate) / SBC Expense",
                limitations=["Actual tax benefits depend on company's tax position"]
            ))

        return results

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key employee compensation metrics"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        metrics = {}

        # Pension metrics
        pension_obligation = balance_sheet.get('pension_obligation', 0)
        pension_assets = balance_sheet.get('pension_plan_assets', 0)

        if pension_obligation > 0:
            metrics['pension_funding_ratio'] = self.safe_divide(pension_assets, pension_obligation)
            metrics['pension_funded_status'] = pension_assets - pension_obligation

        pension_expense = income_statement.get('pension_expense', 0)
        revenue = income_statement.get('revenue', 0)

        if revenue > 0 and pension_expense > 0:
            metrics['pension_cost_intensity'] = self.safe_divide(pension_expense, revenue)

        # SBC metrics
        sbc_expense = income_statement.get('stock_compensation', 0)
        if revenue > 0 and sbc_expense > 0:
            metrics['sbc_intensity'] = self.safe_divide(sbc_expense, revenue)

        # Dilution metrics
        basic_shares = income_statement.get('shares_outstanding_basic', 0)
        diluted_shares = income_statement.get('shares_outstanding_diluted', 0)

        if basic_shares > 0 and diluted_shares > basic_shares:
            metrics['dilution_impact'] = self.safe_divide(diluted_shares - basic_shares, basic_shares)

        # Total compensation intensity
        employee_costs = income_statement.get('employee_costs', 0)
        total_compensation = employee_costs + pension_expense + sbc_expense

        if revenue > 0 and total_compensation > 0:
            metrics['total_compensation_intensity'] = self.safe_divide(total_compensation, revenue)

        return metrics

    def create_post_employment_analysis(self, statements: FinancialStatements) -> PostEmploymentAnalysis:
        """Create comprehensive post-employment benefits analysis object"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        # Extract pension data
        total_pension_obligation = balance_sheet.get('pension_obligation', 0)
        plan_assets_fair_value = balance_sheet.get('pension_plan_assets', 0)
        funded_status = plan_assets_fair_value - total_pension_obligation
        funding_ratio = self.safe_divide(plan_assets_fair_value,
                                         total_pension_obligation) if total_pension_obligation > 0 else 0

        # Plan breakdown
        defined_benefit_obligation = balance_sheet.get('defined_benefit_obligation', total_pension_obligation)
        defined_contribution_assets = balance_sheet.get('defined_contribution_assets', 0)

        # Annual costs
        service_cost = notes.get('pension_service_cost', 0)
        interest_cost = notes.get('pension_interest_cost', 0)
        expected_return_on_assets = notes.get('expected_return_plan_assets', 0)
        net_periodic_cost = income_statement.get('pension_expense', 0)

        # Determine funding status category
        if funding_ratio >= 1.1:
            funding_status_enum = FundingStatus.OVERFUNDED
        elif funding_ratio >= 1.0:
            funding_status_enum = FundingStatus.FULLY_FUNDED
        elif funding_ratio >= 0.8:
            funding_status_enum = FundingStatus.UNDERFUNDED
        else:
            funding_status_enum = FundingStatus.SEVERELY_UNDERFUNDED

        # Risk assessments
        discount_rate = notes.get('pension_discount_rate', 0)
        if discount_rate < 0.04:
            actuarial_assumptions_risk = RiskLevel.HIGH
        elif discount_rate < 0.06:
            actuarial_assumptions_risk = RiskLevel.MODERATE
        else:
            actuarial_assumptions_risk = RiskLevel.LOW

        average_age = notes.get('average_participant_age', 50)
        demographic_risk = RiskLevel.MODERATE if average_age > 55 else RiskLevel.LOW

        # Investment risk based on asset allocation
        equity_allocation = notes.get('pension_equity_allocation', 0.6)  # Default 60%
        investment_risk = RiskLevel.HIGH if equity_allocation > 0.8 else RiskLevel.MODERATE if equity_allocation > 0.5 else RiskLevel.LOW

        return PostEmploymentAnalysis(
            total_pension_obligation=total_pension_obligation,
            plan_assets_fair_value=plan_assets_fair_value,
            funded_status=funded_status,
            funding_ratio=funding_ratio,
            defined_benefit_obligation=defined_benefit_obligation,
            defined_contribution_assets=defined_contribution_assets,
            service_cost=service_cost,
            interest_cost=interest_cost,
            expected_return_on_assets=expected_return_on_assets,
            net_periodic_cost=net_periodic_cost,
            funding_status_enum=funding_status_enum,
            actuarial_assumptions_risk=actuarial_assumptions_risk,
            demographic_risk=demographic_risk,
            investment_risk=investment_risk
        )

    def create_sbc_analysis(self, statements: FinancialStatements) -> ShareBasedCompensationAnalysis:
        """Create comprehensive share-based compensation analysis object"""

        income_statement = statements.income_statement
        notes = statements.notes

        total_sbc_expense = income_statement.get('stock_compensation', 0)
        revenue = income_statement.get('revenue', 0)
        sbc_intensity = self.safe_divide(total_sbc_expense, revenue) if revenue > 0 else 0

        # Composition
        stock_option_expense = notes.get('stock_option_expense', 0)
        restricted_stock_expense = notes.get('restricted_stock_expense', 0)
        performance_share_expense = notes.get('performance_share_expense', 0)

        # Dilution metrics
        basic_shares = income_statement.get('shares_outstanding_basic', 0)
        diluted_shares = income_statement.get('shares_outstanding_diluted', 0)
        potential_dilution = self.safe_divide(diluted_shares - basic_shares, basic_shares) if basic_shares > 0 else 0
        weighted_average_dilutive_shares = diluted_shares - basic_shares

        # Fair value assumptions
        fair_value_assumptions = {
            'volatility': notes.get('sbc_volatility_assumption', 0),
            'risk_free_rate': notes.get('sbc_risk_free_rate', 0),
            'expected_life': notes.get('sbc_expected_life', 0),
            'dividend_yield': notes.get('sbc_dividend_yield', 0)
        }

        # Expense timing
        unvested_value = notes.get('unvested_sbc_value', 0)
        vesting_period = notes.get('weighted_average_vesting_period', 0)

        if unvested_value > 0 and vesting_period > 0:
            expense_timing_pattern = f"${unvested_value:,.0f} to be expensed over {vesting_period:.1f} years"
        else:
            expense_timing_pattern = "Timing information not available"

        # Strategic assessment
        performance_ratio = self.safe_divide(performance_share_expense,
                                             total_sbc_expense) if total_sbc_expense > 0 else 0

        if performance_ratio > 0.4:
            retention_effectiveness = "High performance alignment"
            performance_alignment = RiskLevel.LOW
        elif performance_ratio > 0.2:
            retention_effectiveness = "Moderate performance alignment"
            performance_alignment = RiskLevel.MODERATE
        else:
            retention_effectiveness = "Limited performance alignment"
            performance_alignment = RiskLevel.MODERATE

        return ShareBasedCompensationAnalysis(
            total_sbc_expense=total_sbc_expense,
            sbc_intensity=sbc_intensity,
            stock_option_expense=stock_option_expense,
            restricted_stock_expense=restricted_stock_expense,
            performance_share_expense=performance_share_expense,
            potential_dilution=potential_dilution,
            weighted_average_dilutive_shares=weighted_average_dilutive_shares,
            fair_value_assumptions=fair_value_assumptions,
            expense_timing_pattern=expense_timing_pattern,
            retention_effectiveness=retention_effectiveness,
            performance_alignment=performance_alignment
        )