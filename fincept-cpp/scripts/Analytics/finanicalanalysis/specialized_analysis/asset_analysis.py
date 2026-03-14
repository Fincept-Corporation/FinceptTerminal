
"""
Financial Statement Long-Term Asset Analysis Module
====================================================

Analysis of Long-Term Assets per CFA Institute Curriculum:
- Capitalization vs Expensing decisions
- Depreciation methods and useful life estimates
- Impairment testing and write-downs
- Intangible assets and goodwill
- Asset revaluation under IFRS
- Investment property analysis

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Management discussion and analysis sections
  - Auditor reports and financial statement footnotes
  - Industry benchmarks and competitor data
  - Economic indicators affecting asset valuations

OUTPUT:
  - Asset valuation metrics and quality indicators
  - Depreciation analysis and estimated asset age
  - Impairment risk assessment
  - Capitalization policy evaluation
  - Investment recommendations
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


class DepreciationMethod(Enum):
    """Depreciation methods for PPE"""
    STRAIGHT_LINE = "straight_line"
    DECLINING_BALANCE = "declining_balance"
    DOUBLE_DECLINING = "double_declining_balance"
    SUM_OF_YEARS_DIGITS = "sum_of_years_digits"
    UNITS_OF_PRODUCTION = "units_of_production"


class AmortizationMethod(Enum):
    """Amortization methods for intangibles"""
    STRAIGHT_LINE = "straight_line"
    PATTERN_OF_BENEFITS = "pattern_of_benefits"
    INDEFINITE_LIFE = "indefinite_life"


class ImpairmentModel(Enum):
    """Impairment testing models"""
    US_GAAP_TWO_STEP = "us_gaap_two_step"
    IFRS_ONE_STEP = "ifrs_one_step"
    GOODWILL_QUALITATIVE = "qualitative_assessment"


class AssetCategory(Enum):
    """Categories of long-term assets"""
    PROPERTY_PLANT_EQUIPMENT = "ppe"
    INTANGIBLE_DEFINITE_LIFE = "intangible_definite"
    INTANGIBLE_INDEFINITE_LIFE = "intangible_indefinite"
    GOODWILL = "goodwill"
    INVESTMENT_PROPERTY = "investment_property"
    RIGHT_OF_USE_ASSETS = "rou_assets"


@dataclass
class CapitalizationAnalysis:
    """Analysis of capitalization vs expensing decisions"""
    total_capitalized: float
    total_expensed: float
    capitalization_ratio: float

    # Interest capitalization
    interest_capitalized: float
    interest_expensed: float
    interest_cap_ratio: float

    # R&D capitalization (IFRS only)
    rd_capitalized: float
    rd_expensed: float

    # Policy assessment
    capitalization_aggressiveness: str
    policy_quality_score: float
    concerns: List[str] = field(default_factory=list)


@dataclass
class DepreciationAnalysis:
    """Comprehensive depreciation analysis"""
    depreciation_expense: float
    accumulated_depreciation: float
    gross_ppe: float
    net_ppe: float

    # Derived metrics
    depreciation_rate: float
    average_asset_age: float
    remaining_useful_life: float
    percent_depreciated: float

    # Method assessment
    depreciation_method: DepreciationMethod
    useful_life_estimate: float
    salvage_value_estimate: float

    # Trend indicators
    depreciation_trend: TrendDirection
    capex_to_depreciation: float
    asset_renewal_indicator: str


@dataclass
class IntangibleAssetAnalysis:
    """Analysis of intangible assets"""
    total_intangibles: float
    identifiable_intangibles: float
    goodwill: float

    # Composition
    software: float
    patents_trademarks: float
    customer_relationships: float
    other_intangibles: float

    # Quality metrics
    intangible_intensity: float
    goodwill_to_equity: float
    goodwill_to_assets: float

    # Amortization
    amortization_expense: float
    weighted_average_life: float

    # Impairment history
    cumulative_impairments: float
    impairment_risk_score: float


@dataclass
class ImpairmentAnalysis:
    """Asset impairment analysis"""
    impairment_charges: float
    cumulative_impairments: float

    # By asset category
    ppe_impairments: float
    intangible_impairments: float
    goodwill_impairments: float

    # Risk indicators
    impairment_indicators: List[str]
    recovery_probability: float

    # Testing compliance
    testing_frequency: str
    last_test_date: str
    carrying_value_vs_recoverable: float


@dataclass
class InvestmentPropertyAnalysis:
    """Investment property analysis (IFRS)"""
    investment_property_value: float
    measurement_model: str  # cost or fair_value

    # Fair value metrics
    fair_value: float
    unrealized_gains_losses: float
    rental_income: float

    # Return metrics
    yield_on_investment_property: float
    occupancy_rate: float


class LongTermAssetAnalyzer(BaseAnalyzer):
    """
    Comprehensive long-term asset analyzer implementing CFA Institute standards.
    Covers PPE analysis, intangibles, goodwill, impairment testing, and investment property.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_asset_formulas()
        self._initialize_asset_benchmarks()

    def _initialize_asset_formulas(self):
        """Initialize long-term asset specific formulas"""
        self.formula_registry.update({
            'depreciation_rate': lambda dep_exp, avg_gross_ppe: self.safe_divide(dep_exp, avg_gross_ppe),
            'asset_age': lambda accum_dep, annual_dep: self.safe_divide(accum_dep, annual_dep),
            'remaining_life': lambda net_ppe, annual_dep: self.safe_divide(net_ppe, annual_dep),
            'percent_depreciated': lambda accum_dep, gross_ppe: self.safe_divide(accum_dep, gross_ppe),
            'capex_to_depreciation': lambda capex, dep: self.safe_divide(capex, dep),
            'goodwill_to_equity': lambda goodwill, equity: self.safe_divide(goodwill, equity),
            'intangible_intensity': lambda intangibles, assets: self.safe_divide(intangibles, assets),
            'fixed_asset_turnover': lambda revenue, net_ppe: self.safe_divide(revenue, net_ppe)
        })

    def _initialize_asset_benchmarks(self):
        """Initialize long-term asset benchmarks"""
        self.asset_benchmarks = {
            'percent_depreciated': {
                'new': 0.25, 'moderate': 0.50, 'aged': 0.70, 'fully_depreciated': 0.90
            },
            'capex_to_depreciation': {
                'heavy_investment': 2.0, 'moderate_investment': 1.5, 'maintenance': 1.0, 'underinvestment': 0.7
            },
            'goodwill_to_equity': {
                'low': 0.20, 'moderate': 0.40, 'high': 0.60, 'very_high': 0.80
            },
            'intangible_intensity': {
                'low': 0.10, 'moderate': 0.25, 'high': 0.40, 'very_high': 0.60
            },
            'fixed_asset_turnover': {
                'manufacturing': {'low': 2.0, 'moderate': 4.0, 'high': 6.0},
                'services': {'low': 5.0, 'moderate': 10.0, 'high': 20.0},
                'general': {'low': 3.0, 'moderate': 5.0, 'high': 8.0}
            }
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive long-term asset analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all long-term asset aspects
        """
        results = []

        # Property, Plant & Equipment Analysis
        results.extend(self._analyze_ppe(statements, comparative_data, industry_data))

        # Depreciation Analysis
        results.extend(self._analyze_depreciation(statements, comparative_data))

        # Capitalization vs Expensing Analysis
        results.extend(self._analyze_capitalization_policy(statements, comparative_data))

        # Intangible Assets Analysis
        results.extend(self._analyze_intangible_assets(statements, comparative_data))

        # Goodwill Analysis
        results.extend(self._analyze_goodwill(statements, comparative_data))

        # Impairment Analysis
        results.extend(self._analyze_impairment(statements, comparative_data))

        # Investment Property Analysis (if applicable)
        results.extend(self._analyze_investment_property(statements))

        # IFRS Revaluation Analysis (if applicable)
        if statements.company_info.reporting_standard == ReportingStandard.IFRS:
            results.extend(self._analyze_revaluation(statements, comparative_data))

        return results

    def _analyze_ppe(self, statements: FinancialStatements,
                     comparative_data: Optional[List[FinancialStatements]] = None,
                     industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze Property, Plant & Equipment"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        gross_ppe = balance_sheet.get('ppe_gross', 0)
        accumulated_depreciation = balance_sheet.get('accumulated_depreciation', 0)
        net_ppe = balance_sheet.get('ppe_net', gross_ppe - accumulated_depreciation)
        total_assets = balance_sheet.get('total_assets', 0)
        revenue = income_statement.get('revenue', 0)

        if net_ppe <= 0:
            return results

        # PPE Intensity (Capital Intensity)
        if total_assets > 0:
            ppe_intensity = self.safe_divide(net_ppe, total_assets)

            intensity_interpretation = "Capital-intensive business" if ppe_intensity > 0.4 else \
                                      "Moderate capital intensity" if ppe_intensity > 0.2 else \
                                      "Low capital intensity - asset-light model"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="PPE Intensity",
                value=ppe_intensity,
                interpretation=intensity_interpretation,
                risk_level=RiskLevel.LOW,
                methodology="Net PPE / Total Assets",
                limitations=["Industry-dependent metric"]
            ))

        # Fixed Asset Turnover
        if net_ppe > 0 and revenue > 0:
            # Calculate average net PPE if historical data available
            avg_net_ppe = net_ppe
            if comparative_data and len(comparative_data) > 0:
                prev_net_ppe = comparative_data[-1].balance_sheet.get('ppe_net', 0)
                if prev_net_ppe > 0:
                    avg_net_ppe = (net_ppe + prev_net_ppe) / 2

            fixed_asset_turnover = self.safe_divide(revenue, avg_net_ppe)

            industry_type = industry_data.get('type', 'general') if industry_data else 'general'
            benchmark = self.asset_benchmarks['fixed_asset_turnover'].get(industry_type,
                        self.asset_benchmarks['fixed_asset_turnover']['general'])

            if fixed_asset_turnover >= benchmark['high']:
                turnover_interpretation = "Excellent fixed asset utilization"
                turnover_risk = RiskLevel.LOW
            elif fixed_asset_turnover >= benchmark['moderate']:
                turnover_interpretation = "Good fixed asset utilization"
                turnover_risk = RiskLevel.LOW
            else:
                turnover_interpretation = "Below-average fixed asset utilization - potential overcapacity"
                turnover_risk = RiskLevel.MODERATE

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Fixed Asset Turnover",
                value=fixed_asset_turnover,
                interpretation=turnover_interpretation,
                risk_level=turnover_risk,
                benchmark_comparison=self.compare_to_industry(fixed_asset_turnover,
                                    industry_data.get('fixed_asset_turnover') if industry_data else None),
                methodology="Revenue / Average Net PPE",
                limitations=["Affected by asset age and accounting policies"]
            ))

        # PPE Composition Analysis
        ppe_composition = {
            'land': notes.get('land', 0),
            'buildings': notes.get('buildings', 0),
            'machinery_equipment': notes.get('machinery_equipment', 0),
            'furniture_fixtures': notes.get('furniture_fixtures', 0),
            'construction_in_progress': notes.get('construction_in_progress', 0)
        }

        total_composition = sum(ppe_composition.values())
        if total_composition > 0 and gross_ppe > 0:
            cip_ratio = self.safe_divide(ppe_composition['construction_in_progress'], gross_ppe)

            if cip_ratio > 0.15:
                cip_interpretation = "Significant construction in progress - capacity expansion"
                cip_risk = RiskLevel.MODERATE
            elif cip_ratio > 0.05:
                cip_interpretation = "Moderate construction activity"
                cip_risk = RiskLevel.LOW
            else:
                cip_interpretation = "Limited construction in progress"
                cip_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Construction in Progress Ratio",
                value=cip_ratio,
                interpretation=cip_interpretation,
                risk_level=cip_risk,
                methodology="Construction in Progress / Gross PPE"
            ))

        return results

    def _analyze_depreciation(self, statements: FinancialStatements,
                              comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Comprehensive depreciation analysis"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        notes = statements.notes

        gross_ppe = balance_sheet.get('ppe_gross', 0)
        accumulated_depreciation = balance_sheet.get('accumulated_depreciation', 0)
        net_ppe = balance_sheet.get('ppe_net', gross_ppe - accumulated_depreciation)
        depreciation_expense = income_statement.get('depreciation', 0)

        if depreciation_expense == 0:
            depreciation_expense = cash_flow.get('depreciation_cf', 0)

        if gross_ppe <= 0 or depreciation_expense <= 0:
            return results

        # Percent Depreciated (Asset Age Indicator)
        percent_depreciated = self.safe_divide(accumulated_depreciation, gross_ppe)
        benchmark = self.asset_benchmarks['percent_depreciated']

        if percent_depreciated >= benchmark['fully_depreciated']:
            age_interpretation = "Assets nearly fully depreciated - major replacement cycle likely"
            age_risk = RiskLevel.HIGH
        elif percent_depreciated >= benchmark['aged']:
            age_interpretation = "Aging asset base - increased maintenance and replacement costs expected"
            age_risk = RiskLevel.MODERATE
        elif percent_depreciated >= benchmark['moderate']:
            age_interpretation = "Moderate asset age - normal replacement cycle"
            age_risk = RiskLevel.LOW
        else:
            age_interpretation = "Relatively new asset base"
            age_risk = RiskLevel.LOW

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Percent of Assets Depreciated",
            value=percent_depreciated,
            interpretation=age_interpretation,
            risk_level=age_risk,
            methodology="Accumulated Depreciation / Gross PPE",
            limitations=["Based on historical cost and depreciation policies"]
        ))

        # Average Asset Age
        average_age = self.safe_divide(accumulated_depreciation, depreciation_expense)

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Average Asset Age (Years)",
            value=average_age,
            interpretation=f"Average asset age of {average_age:.1f} years based on depreciation patterns",
            risk_level=RiskLevel.MODERATE if average_age > 10 else RiskLevel.LOW,
            methodology="Accumulated Depreciation / Annual Depreciation Expense",
            limitations=["Assumes consistent depreciation method over time"]
        ))

        # Remaining Useful Life
        remaining_life = self.safe_divide(net_ppe, depreciation_expense)

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Estimated Remaining Useful Life (Years)",
            value=remaining_life,
            interpretation=f"Approximately {remaining_life:.1f} years of remaining useful life at current depreciation rates",
            risk_level=RiskLevel.HIGH if remaining_life < 3 else RiskLevel.MODERATE if remaining_life < 5 else RiskLevel.LOW,
            methodology="Net PPE / Annual Depreciation Expense"
        ))

        # Depreciation Rate
        depreciation_rate = self.safe_divide(depreciation_expense, gross_ppe)

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Annual Depreciation Rate",
            value=depreciation_rate,
            interpretation=f"Annual depreciation rate of {self.format_percentage(depreciation_rate)}",
            risk_level=RiskLevel.LOW,
            methodology="Depreciation Expense / Gross PPE"
        ))

        # CapEx to Depreciation Ratio
        capex = cash_flow.get('capex', 0)
        if capex > 0 and depreciation_expense > 0:
            capex_to_dep = self.safe_divide(capex, depreciation_expense)
            benchmark = self.asset_benchmarks['capex_to_depreciation']

            if capex_to_dep >= benchmark['heavy_investment']:
                capex_interpretation = "Significant investment in new assets - capacity expansion"
                capex_risk = RiskLevel.LOW
            elif capex_to_dep >= benchmark['moderate_investment']:
                capex_interpretation = "Moderate capital investment - growth and maintenance"
                capex_risk = RiskLevel.LOW
            elif capex_to_dep >= benchmark['maintenance']:
                capex_interpretation = "Capital investment approximately matches depreciation - maintenance level"
                capex_risk = RiskLevel.LOW
            else:
                capex_interpretation = "Capital investment below depreciation - potential underinvestment"
                capex_risk = RiskLevel.MODERATE

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="CapEx to Depreciation Ratio",
                value=capex_to_dep,
                interpretation=capex_interpretation,
                risk_level=capex_risk,
                methodology="Capital Expenditures / Depreciation Expense",
                limitations=["Does not distinguish between maintenance and growth capex"]
            ))

        # Depreciation Method Assessment
        dep_method = notes.get('depreciation_method', 'straight_line')
        useful_life = notes.get('average_useful_life', 0)

        if useful_life > 0:
            implied_rate = 1 / useful_life
            actual_rate = depreciation_rate
            rate_difference = abs(actual_rate - implied_rate)

            if rate_difference > 0.02:
                method_interpretation = f"Depreciation rate differs from implied useful life rate - possible accelerated depreciation or policy changes"
            else:
                method_interpretation = f"Depreciation rate consistent with stated {useful_life:.0f}-year useful life"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Depreciation Policy Consistency",
                value=rate_difference,
                interpretation=method_interpretation,
                risk_level=RiskLevel.MODERATE if rate_difference > 0.03 else RiskLevel.LOW,
                methodology="Comparison of actual vs implied depreciation rate"
            ))

        return results

    def _analyze_capitalization_policy(self, statements: FinancialStatements,
                                       comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze capitalization vs expensing decisions"""
        results = []

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        notes = statements.notes

        # Interest Capitalization Analysis
        interest_capitalized = notes.get('interest_capitalized', 0)
        interest_expensed = income_statement.get('interest_expense', 0)
        total_interest = interest_capitalized + interest_expensed

        if total_interest > 0 and interest_capitalized > 0:
            interest_cap_ratio = self.safe_divide(interest_capitalized, total_interest)

            if interest_cap_ratio > 0.3:
                cap_interpretation = "High interest capitalization - may be aggressive"
                cap_risk = RiskLevel.MODERATE
            elif interest_cap_ratio > 0.1:
                cap_interpretation = "Moderate interest capitalization - consistent with construction activity"
                cap_risk = RiskLevel.LOW
            else:
                cap_interpretation = "Conservative interest capitalization policy"
                cap_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Interest Capitalization Ratio",
                value=interest_cap_ratio,
                interpretation=cap_interpretation,
                risk_level=cap_risk,
                methodology="Interest Capitalized / Total Interest Cost",
                limitations=["Should correlate with construction in progress levels"]
            ))

        # R&D Capitalization (IFRS permits, US GAAP generally prohibits)
        rd_expense = income_statement.get('rd_expenses', 0)
        rd_capitalized = notes.get('rd_capitalized', 0)
        total_rd = rd_expense + rd_capitalized

        if total_rd > 0 and rd_capitalized > 0:
            rd_cap_ratio = self.safe_divide(rd_capitalized, total_rd)
            reporting_standard = statements.company_info.reporting_standard

            if reporting_standard == ReportingStandard.IFRS:
                if rd_cap_ratio > 0.5:
                    rd_interpretation = "Aggressive R&D capitalization - scrutinize development phase criteria"
                    rd_risk = RiskLevel.MODERATE
                else:
                    rd_interpretation = "R&D capitalization within typical IFRS practice"
                    rd_risk = RiskLevel.LOW
            else:
                rd_interpretation = "R&D capitalization under US GAAP is unusual - review specific guidance"
                rd_risk = RiskLevel.MODERATE

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="R&D Capitalization Ratio",
                value=rd_cap_ratio,
                interpretation=rd_interpretation,
                risk_level=rd_risk,
                methodology="Capitalized R&D / Total R&D Spending"
            ))

        # Software Development Capitalization
        software_capitalized = notes.get('software_capitalized', 0)
        software_expensed = notes.get('software_expensed', 0)
        total_software = software_capitalized + software_expensed

        if total_software > 0 and software_capitalized > 0:
            software_cap_ratio = self.safe_divide(software_capitalized, total_software)

            if software_cap_ratio > 0.6:
                sw_interpretation = "High software capitalization - review technological feasibility criteria"
                sw_risk = RiskLevel.MODERATE
            else:
                sw_interpretation = "Software capitalization within typical range"
                sw_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Software Capitalization Ratio",
                value=software_cap_ratio,
                interpretation=sw_interpretation,
                risk_level=sw_risk,
                methodology="Capitalized Software / Total Software Costs"
            ))

        # Overall Capitalization Aggressiveness Assessment
        capex = cash_flow.get('capex', 0)
        operating_expenses = income_statement.get('operating_expenses', 0)

        if operating_expenses > 0 and capex > 0:
            capex_to_opex = self.safe_divide(capex, operating_expenses)

            # Compare to historical if available
            if comparative_data and len(comparative_data) >= 2:
                historical_ratios = []
                for past_statements in comparative_data:
                    past_capex = past_statements.cash_flow.get('capex', 0)
                    past_opex = past_statements.income_statement.get('operating_expenses', 0)
                    if past_opex > 0 and past_capex > 0:
                        historical_ratios.append(past_capex / past_opex)

                if historical_ratios:
                    avg_historical = np.mean(historical_ratios)
                    ratio_change = (capex_to_opex - avg_historical) / avg_historical if avg_historical > 0 else 0

                    if ratio_change > 0.2:
                        trend_interpretation = "Significant increase in capitalization relative to expenses"
                        trend_risk = RiskLevel.MODERATE
                    elif ratio_change < -0.2:
                        trend_interpretation = "Decrease in capitalization - more conservative approach"
                        trend_risk = RiskLevel.LOW
                    else:
                        trend_interpretation = "Stable capitalization policy"
                        trend_risk = RiskLevel.LOW

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name="Capitalization Policy Trend",
                        value=ratio_change,
                        interpretation=trend_interpretation,
                        risk_level=trend_risk,
                        methodology="Change in CapEx/OpEx ratio vs historical average"
                    ))

        return results

    def _analyze_intangible_assets(self, statements: FinancialStatements,
                                   comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze intangible assets excluding goodwill"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        intangible_assets = balance_sheet.get('intangible_assets', 0)
        goodwill = balance_sheet.get('goodwill', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        # Separate identifiable intangibles from goodwill
        identifiable_intangibles = intangible_assets - goodwill if intangible_assets > goodwill else intangible_assets

        if identifiable_intangibles <= 0:
            return results

        # Intangible Intensity
        if total_assets > 0:
            intangible_intensity = self.safe_divide(identifiable_intangibles, total_assets)
            benchmark = self.asset_benchmarks['intangible_intensity']

            if intangible_intensity >= benchmark['very_high']:
                intensity_interpretation = "Very high intangible asset intensity - knowledge-based business model"
                intensity_risk = RiskLevel.MODERATE
            elif intangible_intensity >= benchmark['high']:
                intensity_interpretation = "High intangible asset intensity"
                intensity_risk = RiskLevel.LOW
            elif intangible_intensity >= benchmark['moderate']:
                intensity_interpretation = "Moderate intangible assets"
                intensity_risk = RiskLevel.LOW
            else:
                intensity_interpretation = "Low intangible asset base"
                intensity_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Identifiable Intangible Intensity",
                value=intangible_intensity,
                interpretation=intensity_interpretation,
                risk_level=intensity_risk,
                methodology="Identifiable Intangibles / Total Assets"
            ))

        # Intangible Composition
        intangible_composition = {
            'software': notes.get('software_intangibles', 0),
            'patents_trademarks': notes.get('patents_trademarks', 0),
            'customer_relationships': notes.get('customer_relationships', 0),
            'licenses': notes.get('licenses_intangibles', 0),
            'other': notes.get('other_intangibles', 0)
        }

        total_composition = sum(intangible_composition.values())
        if total_composition > 0:
            for category, value in intangible_composition.items():
                if value > 0:
                    category_ratio = self.safe_divide(value, identifiable_intangibles)
                    if category_ratio > 0.1:  # Only report significant categories
                        category_name = category.replace('_', ' ').title()
                        results.append(AnalysisResult(
                            analysis_type=AnalysisType.ACTIVITY,
                            metric_name=f"Intangible Composition - {category_name}",
                            value=category_ratio,
                            interpretation=f"{category_name} represents {self.format_percentage(category_ratio)} of intangibles",
                            risk_level=RiskLevel.LOW,
                            methodology=f"{category_name} / Total Identifiable Intangibles"
                        ))

        # Amortization Analysis
        amortization_expense = income_statement.get('amortization', 0)
        if amortization_expense > 0 and identifiable_intangibles > 0:
            amortization_rate = self.safe_divide(amortization_expense, identifiable_intangibles)
            implied_life = 1 / amortization_rate if amortization_rate > 0 else 0

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Implied Intangible Useful Life",
                value=implied_life,
                interpretation=f"Implied average intangible useful life of {implied_life:.1f} years",
                risk_level=RiskLevel.MODERATE if implied_life > 15 else RiskLevel.LOW,
                methodology="Identifiable Intangibles / Amortization Expense",
                limitations=["May include indefinite-life intangibles not being amortized"]
            ))

        # Internally Generated vs Acquired Intangibles
        acquired_intangibles = notes.get('acquired_intangibles', 0)
        internal_intangibles = notes.get('internal_intangibles', 0)

        if acquired_intangibles > 0 or internal_intangibles > 0:
            total_disclosed = acquired_intangibles + internal_intangibles
            if total_disclosed > 0:
                acquired_ratio = self.safe_divide(acquired_intangibles, total_disclosed)

                if acquired_ratio > 0.8:
                    source_interpretation = "Predominantly acquisition-based intangibles"
                elif acquired_ratio > 0.5:
                    source_interpretation = "Mix of acquired and internally developed intangibles"
                else:
                    source_interpretation = "Predominantly internally generated intangibles"

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Acquired Intangible Ratio",
                    value=acquired_ratio,
                    interpretation=source_interpretation,
                    risk_level=RiskLevel.LOW,
                    methodology="Acquired Intangibles / (Acquired + Internal)"
                ))

        return results

    def _analyze_goodwill(self, statements: FinancialStatements,
                          comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze goodwill and acquisition-related intangibles"""
        results = []

        balance_sheet = statements.balance_sheet
        notes = statements.notes

        goodwill = balance_sheet.get('goodwill', 0)
        total_assets = balance_sheet.get('total_assets', 0)
        total_equity = balance_sheet.get('total_equity', 0)
        intangible_assets = balance_sheet.get('intangible_assets', 0)

        if goodwill <= 0:
            return results

        # Goodwill to Total Assets
        if total_assets > 0:
            goodwill_to_assets = self.safe_divide(goodwill, total_assets)

            if goodwill_to_assets > 0.3:
                gw_interpretation = "Very high goodwill relative to assets - significant acquisition history"
                gw_risk = RiskLevel.HIGH
            elif goodwill_to_assets > 0.2:
                gw_interpretation = "High goodwill concentration - monitor for impairment"
                gw_risk = RiskLevel.MODERATE
            elif goodwill_to_assets > 0.1:
                gw_interpretation = "Moderate goodwill level"
                gw_risk = RiskLevel.LOW
            else:
                gw_interpretation = "Limited goodwill exposure"
                gw_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Goodwill to Total Assets",
                value=goodwill_to_assets,
                interpretation=gw_interpretation,
                risk_level=gw_risk,
                methodology="Goodwill / Total Assets",
                limitations=["Subject to annual impairment testing"]
            ))

        # Goodwill to Equity
        if total_equity > 0:
            goodwill_to_equity = self.safe_divide(goodwill, total_equity)
            benchmark = self.asset_benchmarks['goodwill_to_equity']

            if goodwill_to_equity >= benchmark['very_high']:
                gwe_interpretation = "Goodwill exceeds significant portion of equity - high impairment risk exposure"
                gwe_risk = RiskLevel.HIGH
            elif goodwill_to_equity >= benchmark['high']:
                gwe_interpretation = "High goodwill relative to equity"
                gwe_risk = RiskLevel.MODERATE
            elif goodwill_to_equity >= benchmark['moderate']:
                gwe_interpretation = "Moderate goodwill to equity ratio"
                gwe_risk = RiskLevel.LOW
            else:
                gwe_interpretation = "Low goodwill relative to equity base"
                gwe_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Goodwill to Equity",
                value=goodwill_to_equity,
                interpretation=gwe_interpretation,
                risk_level=gwe_risk,
                methodology="Goodwill / Total Equity",
                limitations=["Impairment could significantly impact equity"]
            ))

        # Goodwill as Portion of Intangibles
        if intangible_assets > 0:
            goodwill_portion = self.safe_divide(goodwill, intangible_assets)

            if goodwill_portion > 0.7:
                portion_interpretation = "Goodwill dominates intangible asset base - synergies from acquisitions"
            elif goodwill_portion > 0.4:
                portion_interpretation = "Significant goodwill among intangible assets"
            else:
                portion_interpretation = "Identifiable intangibles dominate - more transparent value allocation"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Goodwill Portion of Intangibles",
                value=goodwill_portion,
                interpretation=portion_interpretation,
                risk_level=RiskLevel.MODERATE if goodwill_portion > 0.6 else RiskLevel.LOW,
                methodology="Goodwill / Total Intangible Assets"
            ))

        # Goodwill Trend Analysis
        if comparative_data and len(comparative_data) > 0:
            historical_goodwill = []
            for past_statements in comparative_data:
                past_goodwill = past_statements.balance_sheet.get('goodwill', 0)
                historical_goodwill.append(past_goodwill)

            historical_goodwill.append(goodwill)

            if len(historical_goodwill) > 1:
                goodwill_growth = (goodwill / historical_goodwill[0]) - 1 if historical_goodwill[0] > 0 else 0

                if goodwill_growth > 0.2:
                    growth_interpretation = "Significant goodwill growth - active acquisition strategy"
                    growth_risk = RiskLevel.MODERATE
                elif goodwill_growth < -0.1:
                    growth_interpretation = "Goodwill decrease - likely impairment charges"
                    growth_risk = RiskLevel.HIGH
                else:
                    growth_interpretation = "Stable goodwill balance"
                    growth_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Goodwill Growth Rate",
                    value=goodwill_growth,
                    interpretation=growth_interpretation,
                    risk_level=growth_risk,
                    methodology="(Current Goodwill - Historical) / Historical Goodwill"
                ))

        # Goodwill by Reporting Unit (if disclosed)
        reporting_units = notes.get('goodwill_by_segment', {})
        if reporting_units:
            concentration_values = list(reporting_units.values())
            if len(concentration_values) > 1 and sum(concentration_values) > 0:
                max_concentration = max(concentration_values) / sum(concentration_values)

                if max_concentration > 0.7:
                    concentration_interpretation = "Goodwill concentrated in single reporting unit - concentrated impairment risk"
                else:
                    concentration_interpretation = "Goodwill distributed across reporting units"

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Goodwill Concentration",
                    value=max_concentration,
                    interpretation=concentration_interpretation,
                    risk_level=RiskLevel.MODERATE if max_concentration > 0.6 else RiskLevel.LOW,
                    methodology="Largest Reporting Unit Goodwill / Total Goodwill"
                ))

        return results

    def _analyze_impairment(self, statements: FinancialStatements,
                            comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze asset impairment and impairment indicators"""
        results = []

        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet
        notes = statements.notes

        # Current period impairment charges
        impairment_charges = income_statement.get('impairment_losses', 0)
        ppe_impairment = notes.get('ppe_impairment', 0)
        intangible_impairment = notes.get('intangible_impairment', 0)
        goodwill_impairment = notes.get('goodwill_impairment', 0)

        total_impairment = impairment_charges + ppe_impairment + intangible_impairment + goodwill_impairment

        if total_impairment > 0:
            net_income = income_statement.get('net_income', 0)

            if net_income != 0:
                impairment_impact = self.safe_divide(total_impairment, abs(net_income))

                if impairment_impact > 0.5:
                    impact_interpretation = "Major impairment charges significantly impacting earnings"
                    impact_risk = RiskLevel.HIGH
                elif impairment_impact > 0.2:
                    impact_interpretation = "Material impairment impact on earnings"
                    impact_risk = RiskLevel.MODERATE
                else:
                    impact_interpretation = "Moderate impairment impact"
                    impact_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Impairment Impact on Earnings",
                    value=impairment_impact,
                    interpretation=impact_interpretation,
                    risk_level=impact_risk,
                    methodology="Total Impairment Charges / |Net Income|"
                ))

            # Impairment by asset category
            if goodwill_impairment > 0:
                goodwill = balance_sheet.get('goodwill', 0)
                pre_impairment_goodwill = goodwill + goodwill_impairment
                impairment_rate = self.safe_divide(goodwill_impairment, pre_impairment_goodwill)

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Goodwill Impairment Rate",
                    value=impairment_rate,
                    interpretation=f"Goodwill impairment of {self.format_percentage(impairment_rate)} indicates acquisition value deterioration",
                    risk_level=RiskLevel.HIGH if impairment_rate > 0.3 else RiskLevel.MODERATE,
                    methodology="Goodwill Impairment / Pre-Impairment Goodwill"
                ))

        # Impairment Indicators Assessment
        impairment_indicators = self._identify_impairment_indicators(statements, comparative_data)
        if impairment_indicators:
            indicator_count = len(impairment_indicators)

            if indicator_count >= 3:
                indicator_interpretation = "Multiple impairment indicators present - detailed testing required"
                indicator_risk = RiskLevel.HIGH
            elif indicator_count >= 1:
                indicator_interpretation = "Some impairment indicators present - monitoring recommended"
                indicator_risk = RiskLevel.MODERATE
            else:
                indicator_interpretation = "No significant impairment indicators identified"
                indicator_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Impairment Indicator Count",
                value=indicator_count,
                interpretation=indicator_interpretation,
                risk_level=indicator_risk,
                limitations=impairment_indicators,
                methodology="Assessment of qualitative and quantitative impairment triggers"
            ))

        # Historical Impairment Pattern
        if comparative_data and len(comparative_data) >= 2:
            impairment_history = []
            for past_statements in comparative_data:
                past_impairment = past_statements.income_statement.get('impairment_losses', 0)
                impairment_history.append(past_impairment)

            impairment_history.append(total_impairment)

            periods_with_impairment = sum(1 for imp in impairment_history if imp > 0)
            impairment_frequency = periods_with_impairment / len(impairment_history)

            if impairment_frequency > 0.5:
                freq_interpretation = "Frequent impairment charges - potential ongoing asset quality issues"
                freq_risk = RiskLevel.HIGH
            elif impairment_frequency > 0.2:
                freq_interpretation = "Occasional impairment charges"
                freq_risk = RiskLevel.MODERATE
            else:
                freq_interpretation = "Rare impairment history"
                freq_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Impairment Frequency",
                value=impairment_frequency,
                interpretation=freq_interpretation,
                risk_level=freq_risk,
                methodology="Periods with Impairment / Total Periods Analyzed"
            ))

        return results

    def _identify_impairment_indicators(self, statements: FinancialStatements,
                                        comparative_data: Optional[List[FinancialStatements]] = None) -> List[str]:
        """Identify potential impairment indicators per accounting standards"""
        indicators = []

        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet
        notes = statements.notes

        # External indicators
        # Significant decline in market value
        market_cap = notes.get('market_capitalization', 0)
        book_value = balance_sheet.get('total_equity', 0)
        if market_cap > 0 and book_value > 0:
            market_to_book = market_cap / book_value
            if market_to_book < 1.0:
                indicators.append(f"Market value below book value (M/B: {market_to_book:.2f})")

        # Adverse economic conditions (would need external data)

        # Internal indicators
        # Operating losses
        operating_income = income_statement.get('operating_income', 0)
        if operating_income < 0:
            indicators.append("Operating losses indicate potential asset impairment")

        # Cash flow deterioration
        if comparative_data and len(comparative_data) > 0:
            current_ocf = statements.cash_flow.get('operating_cash_flow', 0)
            prev_ocf = comparative_data[-1].cash_flow.get('operating_cash_flow', 0)
            if current_ocf < prev_ocf * 0.7 and prev_ocf > 0:
                indicators.append("Significant decline in operating cash flows")

        # Revenue decline
        if comparative_data and len(comparative_data) > 0:
            current_revenue = income_statement.get('revenue', 0)
            prev_revenue = comparative_data[-1].income_statement.get('revenue', 0)
            if current_revenue < prev_revenue * 0.85 and prev_revenue > 0:
                indicators.append("Significant revenue decline")

        # Goodwill significantly aged without testing
        last_impairment_test = notes.get('last_impairment_test_date', '')
        if not last_impairment_test:
            goodwill = balance_sheet.get('goodwill', 0)
            if goodwill > 0:
                indicators.append("Goodwill present but impairment test date not disclosed")

        # High asset age
        gross_ppe = balance_sheet.get('ppe_gross', 0)
        accumulated_dep = balance_sheet.get('accumulated_depreciation', 0)
        if gross_ppe > 0:
            percent_dep = accumulated_dep / gross_ppe
            if percent_dep > 0.80:
                indicators.append(f"Assets {self.format_percentage(percent_dep)} depreciated - near end of useful life")

        return indicators

    def _analyze_investment_property(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Analyze investment property (primarily IFRS)"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        investment_property = balance_sheet.get('investment_property', 0)

        if investment_property <= 0:
            return results

        total_assets = balance_sheet.get('total_assets', 0)

        # Investment Property Ratio
        if total_assets > 0:
            ip_ratio = self.safe_divide(investment_property, total_assets)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Investment Property Ratio",
                value=ip_ratio,
                interpretation=f"Investment property represents {self.format_percentage(ip_ratio)} of total assets",
                risk_level=RiskLevel.LOW,
                methodology="Investment Property / Total Assets"
            ))

        # Measurement Model
        measurement_model = notes.get('ip_measurement_model', 'cost')

        if measurement_model.lower() == 'fair_value':
            fair_value_gains = notes.get('ip_fair_value_gains', 0)
            fair_value_losses = notes.get('ip_fair_value_losses', 0)
            net_fv_change = fair_value_gains - fair_value_losses

            if investment_property > 0:
                fv_change_ratio = self.safe_divide(net_fv_change, investment_property)

                if abs(fv_change_ratio) > 0.1:
                    fv_interpretation = "Significant fair value changes impacting earnings"
                    fv_risk = RiskLevel.MODERATE
                else:
                    fv_interpretation = "Moderate fair value adjustments"
                    fv_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Investment Property Fair Value Change",
                    value=fv_change_ratio,
                    interpretation=fv_interpretation,
                    risk_level=fv_risk,
                    methodology="Net Fair Value Change / Investment Property Value",
                    limitations=["Fair value model creates earnings volatility"]
                ))

        # Rental Yield
        rental_income = income_statement.get('rental_income', 0)
        if rental_income > 0 and investment_property > 0:
            rental_yield = self.safe_divide(rental_income, investment_property)

            if rental_yield > 0.08:
                yield_interpretation = "High rental yield - strong income generation"
            elif rental_yield > 0.05:
                yield_interpretation = "Moderate rental yield"
            else:
                yield_interpretation = "Low rental yield - value-focused strategy"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Investment Property Rental Yield",
                value=rental_yield,
                interpretation=yield_interpretation,
                risk_level=RiskLevel.LOW,
                methodology="Rental Income / Investment Property Value"
            ))

        return results

    def _analyze_revaluation(self, statements: FinancialStatements,
                             comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze asset revaluation (IFRS revaluation model)"""
        results = []

        balance_sheet = statements.balance_sheet
        notes = statements.notes

        revaluation_surplus = balance_sheet.get('revaluation_surplus', 0)
        total_equity = balance_sheet.get('total_equity', 0)

        if revaluation_surplus <= 0:
            return results

        # Revaluation as portion of equity
        if total_equity > 0:
            reval_to_equity = self.safe_divide(revaluation_surplus, total_equity)

            if reval_to_equity > 0.2:
                reval_interpretation = "Significant revaluation surplus - substantial unrealized gains in assets"
                reval_risk = RiskLevel.MODERATE
            elif reval_to_equity > 0.1:
                reval_interpretation = "Moderate revaluation surplus"
                reval_risk = RiskLevel.LOW
            else:
                reval_interpretation = "Limited revaluation component in equity"
                reval_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Revaluation Surplus Ratio",
                value=reval_to_equity,
                interpretation=reval_interpretation,
                risk_level=reval_risk,
                methodology="Revaluation Surplus / Total Equity",
                limitations=["Under IFRS revaluation model; not available under US GAAP for most assets"]
            ))

        # Revaluation changes
        if comparative_data and len(comparative_data) > 0:
            prev_reval = comparative_data[-1].balance_sheet.get('revaluation_surplus', 0)
            reval_change = revaluation_surplus - prev_reval

            if reval_change != 0:
                change_interpretation = f"Revaluation surplus {'increased' if reval_change > 0 else 'decreased'} by ${abs(reval_change):,.0f}"

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Revaluation Surplus Change",
                    value=reval_change,
                    interpretation=change_interpretation,
                    risk_level=RiskLevel.MODERATE if abs(reval_change) > revaluation_surplus * 0.2 else RiskLevel.LOW,
                    methodology="Current Period - Prior Period Revaluation Surplus"
                ))

        # Assets under revaluation model
        assets_at_revalued = notes.get('assets_revalued_amount', 0)
        assets_at_cost = notes.get('assets_cost_model', 0)

        if assets_at_revalued > 0 and assets_at_cost >= 0:
            total_ppe = assets_at_revalued + assets_at_cost
            revalued_ratio = self.safe_divide(assets_at_revalued, total_ppe)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Assets Under Revaluation Model",
                value=revalued_ratio,
                interpretation=f"{self.format_percentage(revalued_ratio)} of PPE carried at revalued amounts",
                risk_level=RiskLevel.LOW,
                methodology="Revalued Assets / Total PPE"
            ))

        return results

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key long-term asset metrics"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow

        metrics = {}

        gross_ppe = balance_sheet.get('ppe_gross', 0)
        accumulated_dep = balance_sheet.get('accumulated_depreciation', 0)
        net_ppe = balance_sheet.get('ppe_net', gross_ppe - accumulated_dep)
        total_assets = balance_sheet.get('total_assets', 0)
        goodwill = balance_sheet.get('goodwill', 0)
        intangibles = balance_sheet.get('intangible_assets', 0)
        revenue = income_statement.get('revenue', 0)
        depreciation = income_statement.get('depreciation', cash_flow.get('depreciation_cf', 0))
        capex = cash_flow.get('capex', 0)

        # PPE metrics
        if gross_ppe > 0:
            metrics['percent_depreciated'] = self.safe_divide(accumulated_dep, gross_ppe)

        if depreciation > 0:
            metrics['average_asset_age'] = self.safe_divide(accumulated_dep, depreciation)
            metrics['remaining_useful_life'] = self.safe_divide(net_ppe, depreciation)

        if net_ppe > 0 and revenue > 0:
            metrics['fixed_asset_turnover'] = self.safe_divide(revenue, net_ppe)

        if depreciation > 0 and capex > 0:
            metrics['capex_to_depreciation'] = self.safe_divide(capex, depreciation)

        # Intangible metrics
        if total_assets > 0:
            metrics['intangible_intensity'] = self.safe_divide(intangibles, total_assets)
            metrics['goodwill_to_assets'] = self.safe_divide(goodwill, total_assets)

        total_equity = balance_sheet.get('total_equity', 0)
        if total_equity > 0:
            metrics['goodwill_to_equity'] = self.safe_divide(goodwill, total_equity)

        return metrics

    def create_depreciation_analysis(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> DepreciationAnalysis:
        """Create comprehensive depreciation analysis object"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        notes = statements.notes

        gross_ppe = balance_sheet.get('ppe_gross', 0)
        accumulated_depreciation = balance_sheet.get('accumulated_depreciation', 0)
        net_ppe = balance_sheet.get('ppe_net', gross_ppe - accumulated_depreciation)
        depreciation_expense = income_statement.get('depreciation', cash_flow.get('depreciation_cf', 0))
        capex = cash_flow.get('capex', 0)

        # Calculate derived metrics
        depreciation_rate = self.safe_divide(depreciation_expense, gross_ppe)
        percent_depreciated = self.safe_divide(accumulated_depreciation, gross_ppe)
        average_asset_age = self.safe_divide(accumulated_depreciation, depreciation_expense) if depreciation_expense > 0 else 0
        remaining_useful_life = self.safe_divide(net_ppe, depreciation_expense) if depreciation_expense > 0 else 0
        capex_to_depreciation = self.safe_divide(capex, depreciation_expense) if depreciation_expense > 0 else 0

        # Determine depreciation method
        dep_method_str = notes.get('depreciation_method', 'straight_line')
        if 'declining' in dep_method_str.lower():
            depreciation_method = DepreciationMethod.DECLINING_BALANCE
        elif 'double' in dep_method_str.lower():
            depreciation_method = DepreciationMethod.DOUBLE_DECLINING
        elif 'sum' in dep_method_str.lower() or 'syd' in dep_method_str.lower():
            depreciation_method = DepreciationMethod.SUM_OF_YEARS_DIGITS
        elif 'units' in dep_method_str.lower() or 'production' in dep_method_str.lower():
            depreciation_method = DepreciationMethod.UNITS_OF_PRODUCTION
        else:
            depreciation_method = DepreciationMethod.STRAIGHT_LINE

        useful_life_estimate = notes.get('average_useful_life', 0)
        salvage_value_estimate = notes.get('salvage_value', 0)

        # Trend analysis
        depreciation_trend = TrendDirection.STABLE
        if comparative_data and len(comparative_data) >= 2:
            dep_values = []
            for past_statements in comparative_data:
                past_dep = past_statements.income_statement.get('depreciation',
                           past_statements.cash_flow.get('depreciation_cf', 0))
                dep_values.append(past_dep)
            dep_values.append(depreciation_expense)

            if len(dep_values) >= 3:
                if dep_values[-1] > dep_values[0] * 1.1:
                    depreciation_trend = TrendDirection.IMPROVING  # Increasing depreciation
                elif dep_values[-1] < dep_values[0] * 0.9:
                    depreciation_trend = TrendDirection.DETERIORATING  # Decreasing

        # Asset renewal indicator
        if capex_to_depreciation >= 1.5:
            asset_renewal_indicator = "Heavy investment - asset base expanding"
        elif capex_to_depreciation >= 1.0:
            asset_renewal_indicator = "Maintenance level investment"
        elif capex_to_depreciation >= 0.7:
            asset_renewal_indicator = "Below replacement level - aging assets"
        else:
            asset_renewal_indicator = "Significant underinvestment"

        return DepreciationAnalysis(
            depreciation_expense=depreciation_expense,
            accumulated_depreciation=accumulated_depreciation,
            gross_ppe=gross_ppe,
            net_ppe=net_ppe,
            depreciation_rate=depreciation_rate,
            average_asset_age=average_asset_age,
            remaining_useful_life=remaining_useful_life,
            percent_depreciated=percent_depreciated,
            depreciation_method=depreciation_method,
            useful_life_estimate=useful_life_estimate,
            salvage_value_estimate=salvage_value_estimate,
            depreciation_trend=depreciation_trend,
            capex_to_depreciation=capex_to_depreciation,
            asset_renewal_indicator=asset_renewal_indicator
        )

    def create_intangible_analysis(self, statements: FinancialStatements) -> IntangibleAssetAnalysis:
        """Create comprehensive intangible asset analysis object"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        intangible_assets = balance_sheet.get('intangible_assets', 0)
        goodwill = balance_sheet.get('goodwill', 0)
        total_assets = balance_sheet.get('total_assets', 0)
        total_equity = balance_sheet.get('total_equity', 0)

        identifiable_intangibles = intangible_assets - goodwill if intangible_assets > goodwill else intangible_assets

        # Composition
        software = notes.get('software_intangibles', 0)
        patents_trademarks = notes.get('patents_trademarks', 0)
        customer_relationships = notes.get('customer_relationships', 0)
        other_intangibles = notes.get('other_intangibles', 0)

        # Metrics
        intangible_intensity = self.safe_divide(intangible_assets, total_assets)
        goodwill_to_equity = self.safe_divide(goodwill, total_equity) if total_equity > 0 else 0
        goodwill_to_assets = self.safe_divide(goodwill, total_assets)

        # Amortization
        amortization_expense = income_statement.get('amortization', 0)
        weighted_average_life = self.safe_divide(identifiable_intangibles, amortization_expense) if amortization_expense > 0 else 0

        # Impairment
        cumulative_impairments = notes.get('cumulative_intangible_impairments', 0)

        # Impairment risk score
        impairment_risk_score = 0
        if goodwill_to_equity > 0.5:
            impairment_risk_score += 30
        if goodwill_to_assets > 0.2:
            impairment_risk_score += 20
        if intangible_intensity > 0.4:
            impairment_risk_score += 15

        return IntangibleAssetAnalysis(
            total_intangibles=intangible_assets,
            identifiable_intangibles=identifiable_intangibles,
            goodwill=goodwill,
            software=software,
            patents_trademarks=patents_trademarks,
            customer_relationships=customer_relationships,
            other_intangibles=other_intangibles,
            intangible_intensity=intangible_intensity,
            goodwill_to_equity=goodwill_to_equity,
            goodwill_to_assets=goodwill_to_assets,
            amortization_expense=amortization_expense,
            weighted_average_life=weighted_average_life,
            cumulative_impairments=cumulative_impairments,
            impairment_risk_score=impairment_risk_score
        )

    def create_capitalization_analysis(self, statements: FinancialStatements) -> CapitalizationAnalysis:
        """Create capitalization vs expensing analysis object"""

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        notes = statements.notes

        # Interest capitalization
        interest_capitalized = notes.get('interest_capitalized', 0)
        interest_expensed = income_statement.get('interest_expense', 0)
        total_interest = interest_capitalized + interest_expensed
        interest_cap_ratio = self.safe_divide(interest_capitalized, total_interest) if total_interest > 0 else 0

        # R&D
        rd_capitalized = notes.get('rd_capitalized', 0)
        rd_expensed = income_statement.get('rd_expenses', 0)

        # Overall capitalization
        capex = cash_flow.get('capex', 0)
        total_capitalized = capex + interest_capitalized + rd_capitalized
        operating_expenses = income_statement.get('operating_expenses', 0)
        total_expensed = operating_expenses + interest_expensed + rd_expensed

        capitalization_ratio = self.safe_divide(total_capitalized, total_capitalized + total_expensed) if (total_capitalized + total_expensed) > 0 else 0

        # Policy assessment
        concerns = []
        if interest_cap_ratio > 0.4:
            concerns.append("High interest capitalization ratio")
            capitalization_aggressiveness = "Aggressive"
        elif rd_capitalized > rd_expensed * 0.5:
            concerns.append("High R&D capitalization")
            capitalization_aggressiveness = "Aggressive"
        elif interest_cap_ratio > 0.2:
            capitalization_aggressiveness = "Moderate"
        else:
            capitalization_aggressiveness = "Conservative"

        policy_quality_score = 100 - len(concerns) * 20

        return CapitalizationAnalysis(
            total_capitalized=total_capitalized,
            total_expensed=total_expensed,
            capitalization_ratio=capitalization_ratio,
            interest_capitalized=interest_capitalized,
            interest_expensed=interest_expensed,
            interest_cap_ratio=interest_cap_ratio,
            rd_capitalized=rd_capitalized,
            rd_expensed=rd_expensed,
            capitalization_aggressiveness=capitalization_aggressiveness,
            policy_quality_score=policy_quality_score,
            concerns=concerns
        )
