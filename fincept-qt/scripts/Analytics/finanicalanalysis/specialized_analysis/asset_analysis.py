
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
        notes = statements.notes

        net_ppe = balance_sheet.get('ppe_net')
        gross_ppe = balance_sheet.get('ppe_gross')
        total_assets = balance_sheet.get('total_assets')
        revenue = statements.income_statement.get('revenue')

        if net_ppe is None or net_ppe <= 0:
            results.append(self.unavailable(AnalysisType.ACTIVITY, "PPE Analysis",
                                            "net PPE not reported or not positive"))
            return results

        if total_assets and total_assets > 0:
            ppe_intensity = net_ppe / total_assets
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

        if revenue and revenue > 0:
            avg_net_ppe, basis = self.average_balance(statements, comparative_data, 'ppe_net')
            fixed_asset_turnover = revenue / avg_net_ppe
            industry_type = str(industry_data.get('type', 'general')).lower() if isinstance(industry_data, dict) else 'general'
            benchmark = self.asset_benchmarks['fixed_asset_turnover'].get(
                industry_type, self.asset_benchmarks['fixed_asset_turnover']['general'])
            if fixed_asset_turnover >= benchmark['high']:
                turnover_interpretation, turnover_risk = "Excellent fixed asset utilization", RiskLevel.LOW
            elif fixed_asset_turnover >= benchmark['moderate']:
                turnover_interpretation, turnover_risk = "Good fixed asset utilization", RiskLevel.LOW
            else:
                turnover_interpretation, turnover_risk = "Below-average fixed asset utilization - potential overcapacity", RiskLevel.MODERATE
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Fixed Asset Turnover",
                value=fixed_asset_turnover,
                interpretation=turnover_interpretation,
                risk_level=turnover_risk,
                benchmark_comparison=self.compare_to_industry(fixed_asset_turnover,
                                    industry_data.get('fixed_asset_turnover') if industry_data else None),
                methodology=f"Revenue / Net PPE ({self.basis_note(basis)})",
                limitations=["Affected by asset age and accounting policies"]
            ))

        cip = self.note_number(notes, 'construction_in_progress')
        if cip is not None and gross_ppe and gross_ppe > 0:
            cip_ratio = cip / gross_ppe
            if cip_ratio > 0.15:
                cip_interpretation, cip_risk = "Significant construction in progress - capacity expansion", RiskLevel.MODERATE
            elif cip_ratio > 0.05:
                cip_interpretation, cip_risk = "Moderate construction activity", RiskLevel.LOW
            else:
                cip_interpretation, cip_risk = "Limited construction in progress", RiskLevel.LOW
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
        """CFA long-lived asset age metrics"""
        results = []
        balance_sheet = statements.balance_sheet
        cash_flow = statements.cash_flow
        notes = statements.notes

        gross_ppe = balance_sheet.get('ppe_gross')
        accumulated_depreciation = balance_sheet.get('accumulated_depreciation')
        net_ppe = balance_sheet.get('ppe_net')
        depreciation_expense = self._depreciation_expense(statements)
        dep_limit = ["Uses the reported depreciation line; if it includes amortization of intangibles "
                     "the age and life estimates are understated"]

        if depreciation_expense is None or depreciation_expense <= 0:
            results.append(self.unavailable(AnalysisType.QUALITY, "Depreciation Analysis",
                                            "depreciation expense not reported or not positive"))
            return results

        if gross_ppe and gross_ppe > 0 and accumulated_depreciation is not None:
            percent_depreciated = accumulated_depreciation / gross_ppe
            benchmark = self.asset_benchmarks['percent_depreciated']
            if percent_depreciated >= benchmark['fully_depreciated']:
                age_interpretation, age_risk = "Assets nearly fully depreciated - major replacement cycle likely", RiskLevel.HIGH
            elif percent_depreciated >= benchmark['aged']:
                age_interpretation, age_risk = "Aging asset base - increased maintenance and replacement costs expected", RiskLevel.MODERATE
            elif percent_depreciated >= benchmark['moderate']:
                age_interpretation, age_risk = "Moderate asset age - normal replacement cycle", RiskLevel.LOW
            else:
                age_interpretation, age_risk = "Relatively new asset base", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Percent of Assets Depreciated",
                value=percent_depreciated,
                interpretation=age_interpretation,
                risk_level=age_risk,
                methodology="Accumulated Depreciation / Gross PPE",
                limitations=["Based on historical cost and depreciation policies"]
            ))

            average_age = accumulated_depreciation / depreciation_expense
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Average Asset Age (Years)",
                value=average_age,
                interpretation=f"Average asset age of {average_age:.1f} years based on depreciation patterns",
                risk_level=RiskLevel.MODERATE if average_age > 10 else RiskLevel.LOW,
                methodology="Accumulated Depreciation / Annual Depreciation Expense",
                limitations=["Assumes straight-line depreciation and a stable asset base"] + dep_limit
            ))

            total_life = gross_ppe / depreciation_expense
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Estimated Total Useful Life (Years)",
                value=total_life,
                interpretation=f"Gross PPE implies an average depreciable life of {total_life:.1f} years",
                risk_level=RiskLevel.LOW,
                methodology="Gross PPE / Annual Depreciation Expense",
                limitations=["Land is not depreciated and lengthens this estimate"] + dep_limit
            ))
        else:
            results.append(self.unavailable(AnalysisType.QUALITY, "Average Asset Age (Years)",
                                            "gross PPE and accumulated depreciation (or net PPE) are required",
                                            "Accumulated Depreciation / Annual Depreciation Expense"))

        if net_ppe is not None and net_ppe > 0:
            remaining_life = net_ppe / depreciation_expense
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Estimated Remaining Useful Life (Years)",
                value=remaining_life,
                interpretation=f"Approximately {remaining_life:.1f} years of remaining useful life at current depreciation rates",
                risk_level=RiskLevel.HIGH if remaining_life < 3 else RiskLevel.MODERATE if remaining_life < 5 else RiskLevel.LOW,
                methodology="Net PPE / Annual Depreciation Expense",
                limitations=dep_limit
            ))

        if gross_ppe and gross_ppe > 0:
            depreciation_rate = depreciation_expense / gross_ppe
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Annual Depreciation Rate",
                value=depreciation_rate,
                interpretation=f"Annual depreciation rate of {self.format_percentage(depreciation_rate)}",
                risk_level=RiskLevel.LOW,
                methodology="Depreciation Expense / Gross PPE (ending)"
            ))

            useful_life = self.note_number(notes, 'average_useful_life')
            if useful_life and useful_life > 0:
                rate_difference = abs(depreciation_rate - 1 / useful_life)
                method_interpretation = ("Depreciation rate differs from implied useful life rate - possible accelerated depreciation or policy changes"
                                         if rate_difference > 0.02 else
                                         f"Depreciation rate consistent with stated {useful_life:.0f}-year useful life")
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Depreciation Policy Consistency",
                    value=rate_difference,
                    interpretation=method_interpretation,
                    risk_level=RiskLevel.MODERATE if rate_difference > 0.03 else RiskLevel.LOW,
                    methodology="|Depreciation Expense / Gross PPE - 1 / Stated Useful Life|"
                ))

        capex = cash_flow.get('capex')
        if capex is not None and capex > 0:
            capex_to_dep = capex / depreciation_expense
            benchmark = self.asset_benchmarks['capex_to_depreciation']
            if capex_to_dep >= benchmark['heavy_investment']:
                capex_interpretation, capex_risk = "Significant investment in new assets - capacity expansion", RiskLevel.LOW
            elif capex_to_dep >= benchmark['moderate_investment']:
                capex_interpretation, capex_risk = "Moderate capital investment - growth and maintenance", RiskLevel.LOW
            elif capex_to_dep >= benchmark['maintenance']:
                capex_interpretation, capex_risk = "Capital investment approximately matches depreciation - maintenance level", RiskLevel.LOW
            else:
                capex_interpretation, capex_risk = "Capital investment below depreciation - potential underinvestment", RiskLevel.MODERATE
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="CapEx to Depreciation Ratio",
                value=capex_to_dep,
                interpretation=capex_interpretation,
                risk_level=capex_risk,
                methodology="Capital Expenditures / Depreciation Expense",
                limitations=["Does not distinguish between maintenance and growth capex"]
            ))

        return results

    def _analyze_capitalization_policy(self, statements: FinancialStatements,
                                       comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Capitalization vs expensing decisions (note disclosures)"""
        results = []
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        notes = statements.notes

        interest_capitalized = self.note_number(notes, 'interest_capitalized')
        interest_expensed = income_statement.get('interest_expense', 0.0)
        if interest_capitalized and interest_capitalized > 0:
            interest_cap_ratio = interest_capitalized / (interest_capitalized + interest_expensed)
            if interest_cap_ratio > 0.3:
                cap_interpretation, cap_risk = "High interest capitalization - may be aggressive", RiskLevel.MODERATE
            elif interest_cap_ratio > 0.1:
                cap_interpretation, cap_risk = "Moderate interest capitalization - consistent with construction activity", RiskLevel.LOW
            else:
                cap_interpretation, cap_risk = "Conservative interest capitalization policy", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Interest Capitalization Ratio",
                value=interest_cap_ratio,
                interpretation=cap_interpretation,
                risk_level=cap_risk,
                methodology="Interest Capitalized / Total Interest Cost",
                limitations=["Should correlate with construction in progress levels"]
            ))

        rd_capitalized = self.note_number(notes, 'rd_capitalized')
        if rd_capitalized and rd_capitalized > 0:
            rd_cap_ratio = rd_capitalized / (rd_capitalized + income_statement.get('rd_expenses', 0.0))
            if statements.company_info.reporting_standard == ReportingStandard.IFRS:
                if rd_cap_ratio > 0.5:
                    rd_interpretation, rd_risk = "Aggressive R&D capitalization - scrutinize development phase criteria", RiskLevel.MODERATE
                else:
                    rd_interpretation, rd_risk = "R&D capitalization within typical IFRS practice", RiskLevel.LOW
            else:
                rd_interpretation, rd_risk = "R&D capitalization under US GAAP is unusual - review specific guidance", RiskLevel.MODERATE
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="R&D Capitalization Ratio",
                value=rd_cap_ratio,
                interpretation=rd_interpretation,
                risk_level=rd_risk,
                methodology="Capitalized R&D / Total R&D Spending"
            ))

        software_capitalized = self.note_number(notes, 'software_capitalized')
        if software_capitalized and software_capitalized > 0:
            software_cap_ratio = software_capitalized / (software_capitalized
                                                         + (self.note_number(notes, 'software_expensed') or 0.0))
            if software_cap_ratio > 0.6:
                sw_interpretation, sw_risk = "High software capitalization - review technological feasibility criteria", RiskLevel.MODERATE
            else:
                sw_interpretation, sw_risk = "Software capitalization within typical range", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Software Capitalization Ratio",
                value=software_cap_ratio,
                interpretation=sw_interpretation,
                risk_level=sw_risk,
                methodology="Capitalized Software / Total Software Costs"
            ))

        capex = cash_flow.get('capex')
        operating_expenses = income_statement.get('operating_expenses')
        if capex and capex > 0 and operating_expenses and operating_expenses > 0 \
                and comparative_data and len(comparative_data) >= 2:
            capex_to_opex = capex / operating_expenses
            historical_ratios = []
            for past in comparative_data:
                past_capex = past.cash_flow.get('capex')
                past_opex = past.income_statement.get('operating_expenses')
                if past_capex and past_capex > 0 and past_opex and past_opex > 0:
                    historical_ratios.append(past_capex / past_opex)
            if historical_ratios:
                avg_historical = float(np.mean(historical_ratios))
                ratio_change = (capex_to_opex - avg_historical) / avg_historical
                if ratio_change > 0.2:
                    trend_interpretation, trend_risk = "Significant increase in capitalization relative to expenses", RiskLevel.MODERATE
                elif ratio_change < -0.2:
                    trend_interpretation, trend_risk = "Decrease in capitalization - more conservative approach", RiskLevel.LOW
                else:
                    trend_interpretation, trend_risk = "Stable capitalization policy", RiskLevel.LOW
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
        """Identifiable intangible assets. Convention: 'intangible_assets' EXCLUDES goodwill."""
        results = []
        balance_sheet = statements.balance_sheet
        notes = statements.notes

        identifiable_intangibles = balance_sheet.get('intangible_assets')
        total_assets = balance_sheet.get('total_assets')
        if identifiable_intangibles is None or identifiable_intangibles <= 0:
            return results

        if total_assets and total_assets > 0:
            intangible_intensity = identifiable_intangibles / total_assets
            benchmark = self.asset_benchmarks['intangible_intensity']
            if intangible_intensity >= benchmark['very_high']:
                intensity_interpretation, intensity_risk = "Very high intangible asset intensity - knowledge-based business model", RiskLevel.MODERATE
            elif intangible_intensity >= benchmark['high']:
                intensity_interpretation, intensity_risk = "High intangible asset intensity", RiskLevel.LOW
            elif intangible_intensity >= benchmark['moderate']:
                intensity_interpretation, intensity_risk = "Moderate intangible assets", RiskLevel.LOW
            else:
                intensity_interpretation, intensity_risk = "Low intangible asset base", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Identifiable Intangible Intensity",
                value=intangible_intensity,
                interpretation=intensity_interpretation,
                risk_level=intensity_risk,
                methodology="Identifiable Intangibles (excluding goodwill) / Total Assets"
            ))

        composition = {
            'software': self.note_number(notes, 'software_intangibles'),
            'patents_trademarks': self.note_number(notes, 'patents_trademarks'),
            'customer_relationships': self.note_number(notes, 'customer_relationships'),
            'licenses': self.note_number(notes, 'licenses_intangibles'),
            'other': self.note_number(notes, 'other_intangibles'),
        }
        for category, value in composition.items():
            if value and value > 0:
                category_ratio = value / identifiable_intangibles
                if category_ratio > 0.1:
                    category_name = category.replace('_', ' ').title()
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.ACTIVITY,
                        metric_name=f"Intangible Composition - {category_name}",
                        value=category_ratio,
                        interpretation=f"{category_name} represents {self.format_percentage(category_ratio)} of intangibles",
                        risk_level=RiskLevel.LOW,
                        methodology=f"{category_name} / Total Identifiable Intangibles"
                    ))

        amortization_expense = statements.income_statement.get('amortization')
        if amortization_expense and amortization_expense > 0:
            implied_life = identifiable_intangibles / amortization_expense
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Implied Intangible Useful Life",
                value=implied_life,
                interpretation=f"Carrying amount covers {implied_life:.1f} more years of amortization at the current rate",
                risk_level=RiskLevel.MODERATE if implied_life > 15 else RiskLevel.LOW,
                methodology="Identifiable Intangibles (net) / Amortization Expense (remaining life)",
                limitations=["May include indefinite-life intangibles not being amortized"]
            ))

        acquired = self.note_number(notes, 'acquired_intangibles') or 0.0
        internal = self.note_number(notes, 'internal_intangibles') or 0.0
        if acquired + internal > 0:
            acquired_ratio = acquired / (acquired + internal)
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

        goodwill = balance_sheet.get('goodwill')
        total_assets = balance_sheet.get('total_assets')
        total_equity = balance_sheet.get('total_equity')
        intangible_assets = balance_sheet.get('intangible_assets')

        if goodwill is None or goodwill <= 0:
            return results

        if total_assets and total_assets > 0:
            goodwill_to_assets = goodwill / total_assets
            if goodwill_to_assets > 0.3:
                gw_interpretation, gw_risk = "Very high goodwill relative to assets - significant acquisition history", RiskLevel.HIGH
            elif goodwill_to_assets > 0.2:
                gw_interpretation, gw_risk = "High goodwill concentration - monitor for impairment", RiskLevel.MODERATE
            elif goodwill_to_assets > 0.1:
                gw_interpretation, gw_risk = "Moderate goodwill level", RiskLevel.LOW
            else:
                gw_interpretation, gw_risk = "Limited goodwill exposure", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Goodwill to Total Assets",
                value=goodwill_to_assets,
                interpretation=gw_interpretation,
                risk_level=gw_risk,
                methodology="Goodwill / Total Assets",
                limitations=["Subject to annual impairment testing"]
            ))

        if total_equity is not None:
            if total_equity > 0:
                goodwill_to_equity = goodwill / total_equity
                benchmark = self.asset_benchmarks['goodwill_to_equity']
                if goodwill_to_equity >= benchmark['very_high']:
                    gwe_interpretation, gwe_risk = "Goodwill exceeds significant portion of equity - high impairment risk exposure", RiskLevel.HIGH
                elif goodwill_to_equity >= benchmark['high']:
                    gwe_interpretation, gwe_risk = "High goodwill relative to equity", RiskLevel.MODERATE
                elif goodwill_to_equity >= benchmark['moderate']:
                    gwe_interpretation, gwe_risk = "Moderate goodwill to equity ratio", RiskLevel.LOW
                else:
                    gwe_interpretation, gwe_risk = "Low goodwill relative to equity base", RiskLevel.LOW
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Goodwill to Equity",
                    value=goodwill_to_equity,
                    interpretation=gwe_interpretation,
                    risk_level=gwe_risk,
                    methodology="Goodwill / Total Equity",
                    limitations=["Impairment could significantly impact equity"]
                ))
            else:
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Goodwill to Equity",
                    value=None,
                    interpretation="Not meaningful: equity is zero or negative while goodwill is carried - any impairment deepens the deficit",
                    risk_level=RiskLevel.HIGH,
                    methodology="Goodwill / Total Equity"
                ))

        # Share of goodwill in all intangibles (intangible_assets excludes goodwill)
        total_intangibles = goodwill + (intangible_assets or 0.0)
        goodwill_portion = goodwill / total_intangibles
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
            methodology="Goodwill / (Goodwill + Identifiable Intangible Assets)"
        ))

        if comparative_data:
            history = [p.balance_sheet['goodwill'] for p in comparative_data if 'goodwill' in p.balance_sheet]
            if history:
                base = history[0]
                goodwill_growth = self.growth(goodwill, base)
                if goodwill_growth is None:
                    results.append(self.unavailable(AnalysisType.QUALITY, "Goodwill Growth Rate",
                                                    "no goodwill at the start of the window (new acquisitions)",
                                                    "(Current Goodwill - Earliest Goodwill) / Earliest Goodwill"))
                else:
                    if goodwill_growth > 0.2:
                        growth_interpretation, growth_risk = "Significant goodwill growth - active acquisition strategy", RiskLevel.MODERATE
                    elif goodwill_growth < -0.1:
                        growth_interpretation, growth_risk = "Goodwill decrease - likely impairment charges or disposals", RiskLevel.HIGH
                    else:
                        growth_interpretation, growth_risk = "Stable goodwill balance", RiskLevel.LOW
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name="Goodwill Growth Rate",
                        value=goodwill_growth,
                        interpretation=growth_interpretation,
                        risk_level=growth_risk,
                        methodology=f"(Current Goodwill - Earliest Goodwill) / Earliest Goodwill over {len(history)} prior periods"
                    ))

        reporting_units = notes.get('goodwill_by_segment')
        if isinstance(reporting_units, dict):
            values = [v for v in reporting_units.values() if isinstance(v, (int, float)) and v > 0]
            if len(values) > 1:
                max_concentration = max(values) / sum(values)
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Goodwill Concentration",
                    value=max_concentration,
                    interpretation=("Goodwill concentrated in single reporting unit - concentrated impairment risk"
                                    if max_concentration > 0.7 else "Goodwill distributed across reporting units"),
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

        parts = {
            'income statement': income_statement.get('impairment_losses'),
            'PPE note': self.note_number(notes, 'ppe_impairment'),
            'intangibles note': self.note_number(notes, 'intangible_impairment'),
            'goodwill note': self.note_number(notes, 'goodwill_impairment'),
        }
        # The notes break down the income-statement charge; use whichever is larger to avoid double counting.
        note_total = sum(abs(v) for k, v in parts.items() if k != 'income statement' and v)
        total_impairment = max(abs(parts['income statement'] or 0.0), note_total)

        net_income = income_statement.get('net_income')
        if total_impairment > 0 and net_income:
            impairment_impact = total_impairment / abs(net_income)
            if impairment_impact > 0.5:
                impact_interpretation, impact_risk = "Major impairment charges significantly impacting earnings", RiskLevel.HIGH
            elif impairment_impact > 0.2:
                impact_interpretation, impact_risk = "Material impairment impact on earnings", RiskLevel.MODERATE
            else:
                impact_interpretation, impact_risk = "Moderate impairment impact", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Impairment Impact on Earnings",
                value=impairment_impact,
                interpretation=impact_interpretation,
                risk_level=impact_risk,
                methodology="Total Impairment Charges / |Net Income|"
            ))

        goodwill_impairment = parts['goodwill note']
        if goodwill_impairment and goodwill_impairment > 0:
            pre_impairment_goodwill = balance_sheet.get('goodwill', 0.0) + goodwill_impairment
            impairment_rate = goodwill_impairment / pre_impairment_goodwill
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Goodwill Impairment Rate",
                value=impairment_rate,
                interpretation=f"Goodwill impairment of {self.format_percentage(impairment_rate)} indicates acquisition value deterioration",
                risk_level=RiskLevel.HIGH if impairment_rate > 0.3 else RiskLevel.MODERATE,
                methodology="Goodwill Impairment / Pre-Impairment Goodwill"
            ))

        impairment_indicators = self._identify_impairment_indicators(statements, comparative_data)
        if impairment_indicators:
            indicator_count = len(impairment_indicators)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Impairment Indicator Count",
                value=indicator_count,
                interpretation=("Multiple impairment indicators present - detailed testing required" if indicator_count >= 3
                                else "Some impairment indicators present - monitoring recommended"),
                risk_level=RiskLevel.HIGH if indicator_count >= 3 else RiskLevel.MODERATE,
                limitations=impairment_indicators,
                methodology="Assessment of qualitative and quantitative impairment triggers"
            ))

        if comparative_data and len(comparative_data) >= 2:
            history = [p.income_statement['impairment_losses'] for p in comparative_data
                       if 'impairment_losses' in p.income_statement]
            if history and 'impairment_losses' in income_statement:
                history.append(income_statement['impairment_losses'])
                impairment_frequency = sum(1 for imp in history if abs(imp) > 0) / len(history)
                if impairment_frequency > 0.5:
                    freq_interpretation, freq_risk = "Frequent impairment charges - potential ongoing asset quality issues", RiskLevel.HIGH
                elif impairment_frequency > 0.2:
                    freq_interpretation, freq_risk = "Occasional impairment charges", RiskLevel.MODERATE
                else:
                    freq_interpretation, freq_risk = "Rare impairment history", RiskLevel.LOW
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Impairment Frequency",
                    value=impairment_frequency,
                    interpretation=freq_interpretation,
                    risk_level=freq_risk,
                    methodology="Periods with Impairment / Periods reporting impairment"
                ))

        return results

    def _identify_impairment_indicators(self, statements: FinancialStatements,
                                        comparative_data: Optional[List[FinancialStatements]] = None) -> List[str]:
        """Impairment triggers (IAS 36 / ASC 360) that the supplied data can show"""
        indicators = []
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet
        notes = statements.notes

        market_cap = self.note_number(notes, 'market_capitalization', statements.company_info.market_cap)
        book_value = balance_sheet.get('total_equity')
        if market_cap and market_cap > 0 and book_value and book_value > 0 and market_cap / book_value < 1.0:
            indicators.append(f"Market value below book value (M/B: {market_cap / book_value:.2f})")

        operating_income = income_statement.get('operating_income')
        if operating_income is not None and operating_income < 0:
            indicators.append("Operating losses indicate potential asset impairment")

        if comparative_data:
            current_ocf = statements.cash_flow.get('operating_cash_flow')
            prev_ocf = self.prior_value(comparative_data, 'cash_flow', 'operating_cash_flow')
            if current_ocf is not None and prev_ocf and prev_ocf > 0 and current_ocf < prev_ocf * 0.7:
                indicators.append("Significant decline in operating cash flows")

            current_revenue = income_statement.get('revenue')
            prev_revenue = self.prior_value(comparative_data, 'income_statement', 'revenue')
            if current_revenue is not None and prev_revenue and prev_revenue > 0 and current_revenue < prev_revenue * 0.85:
                indicators.append("Significant revenue decline")

        gross_ppe = balance_sheet.get('ppe_gross')
        accumulated_dep = balance_sheet.get('accumulated_depreciation')
        if gross_ppe and gross_ppe > 0 and accumulated_dep is not None and accumulated_dep / gross_ppe > 0.80:
            indicators.append(f"Assets {self.format_percentage(accumulated_dep / gross_ppe)} depreciated - near end of useful life")

        return indicators

    def _analyze_investment_property(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Investment property (IAS 40)"""
        results = []
        balance_sheet = statements.balance_sheet
        notes = statements.notes

        investment_property = balance_sheet.get('investment_property', self.note_number(notes, 'investment_property'))
        if not investment_property or investment_property <= 0:
            return results

        total_assets = balance_sheet.get('total_assets')
        if total_assets and total_assets > 0:
            ip_ratio = investment_property / total_assets
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Investment Property Ratio",
                value=ip_ratio,
                interpretation=f"Investment property represents {self.format_percentage(ip_ratio)} of total assets",
                risk_level=RiskLevel.LOW,
                methodology="Investment Property / Total Assets"
            ))

        if self.note_text(notes, 'ip_measurement_model', 'cost') == 'fair_value':
            net_fv_change = ((self.note_number(notes, 'ip_fair_value_gains') or 0.0)
                             - (self.note_number(notes, 'ip_fair_value_losses') or 0.0))
            fv_change_ratio = net_fv_change / investment_property
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Investment Property Fair Value Change",
                value=fv_change_ratio,
                interpretation=("Significant fair value changes impacting earnings" if abs(fv_change_ratio) > 0.1
                                else "Moderate fair value adjustments"),
                risk_level=RiskLevel.MODERATE if abs(fv_change_ratio) > 0.1 else RiskLevel.LOW,
                methodology="Net Fair Value Change / Investment Property Value",
                limitations=["Fair value model creates earnings volatility"]
            ))

        rental_income = statements.income_statement.get('rental_income', self.note_number(notes, 'rental_income'))
        if rental_income and rental_income > 0:
            rental_yield = rental_income / investment_property
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Investment Property Rental Yield",
                value=rental_yield,
                interpretation=("High rental yield - strong income generation" if rental_yield > 0.08
                                else "Moderate rental yield" if rental_yield > 0.05
                                else "Low rental yield - value-focused strategy"),
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
        assets_at_revalued = self.note_number(notes, 'assets_revalued_amount', 0.0)
        assets_at_cost = self.note_number(notes, 'assets_cost_model', 0.0)

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

    def get_key_metrics(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        unavailable: Optional[Dict[str, str]] = None) -> Dict[str, float]:
        """Key long-term asset metrics (missing inputs -> metric omitted)"""
        bs = statements.balance_sheet
        metrics = {}

        gross_ppe = bs.get('ppe_gross')
        accumulated_dep = bs.get('accumulated_depreciation')
        net_ppe = bs.get('ppe_net')
        total_assets = bs.get('total_assets')
        goodwill = bs.get('goodwill')
        intangibles = bs.get('intangible_assets')
        revenue = statements.income_statement.get('revenue')
        depreciation = self._depreciation_expense(statements)
        capex = statements.cash_flow.get('capex')

        if gross_ppe and gross_ppe > 0 and accumulated_dep is not None:
            metrics['percent_depreciated'] = accumulated_dep / gross_ppe
        if depreciation and depreciation > 0:
            if accumulated_dep is not None:
                metrics['average_asset_age'] = accumulated_dep / depreciation
            if net_ppe is not None:
                metrics['remaining_useful_life'] = net_ppe / depreciation
            if capex is not None:
                metrics['capex_to_depreciation'] = capex / depreciation
        if net_ppe and net_ppe > 0 and revenue and revenue > 0:
            avg_ppe, _ = self.average_balance(statements, comparative_data, 'ppe_net')
            metrics['fixed_asset_turnover'] = revenue / avg_ppe
        if total_assets and total_assets > 0:
            if intangibles is not None:
                metrics['intangible_intensity'] = intangibles / total_assets
            if goodwill is not None:
                metrics['goodwill_to_assets'] = goodwill / total_assets
        total_equity = bs.get('total_equity')
        if goodwill is not None and total_equity and total_equity > 0:
            metrics['goodwill_to_equity'] = goodwill / total_equity

        return metrics

    def create_depreciation_analysis(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> DepreciationAnalysis:
        """Depreciation analysis object (None where not computable)"""
        bs = statements.balance_sheet
        notes = statements.notes

        gross_ppe = bs.get('ppe_gross')
        accumulated_depreciation = bs.get('accumulated_depreciation')
        net_ppe = bs.get('ppe_net')
        depreciation_expense = self._depreciation_expense(statements)
        capex = statements.cash_flow.get('capex')

        def div(a, b):
            return a / b if a is not None and b else None

        depreciation_rate = div(depreciation_expense, gross_ppe)
        percent_depreciated = div(accumulated_depreciation, gross_ppe)
        average_asset_age = div(accumulated_depreciation, depreciation_expense)
        remaining_useful_life = div(net_ppe, depreciation_expense)
        capex_to_depreciation = div(capex, depreciation_expense)

        dep_method_str = self.note_text(notes, 'depreciation_method', 'straight_line')
        if 'double' in dep_method_str:
            depreciation_method = DepreciationMethod.DOUBLE_DECLINING
        elif 'declining' in dep_method_str:
            depreciation_method = DepreciationMethod.DECLINING_BALANCE
        elif 'sum' in dep_method_str or 'syd' in dep_method_str:
            depreciation_method = DepreciationMethod.SUM_OF_YEARS_DIGITS
        elif 'units' in dep_method_str or 'production' in dep_method_str:
            depreciation_method = DepreciationMethod.UNITS_OF_PRODUCTION
        else:
            depreciation_method = DepreciationMethod.STRAIGHT_LINE

        depreciation_trend = TrendDirection.STABLE
        if comparative_data and len(comparative_data) >= 2 and depreciation_expense is not None:
            dep_values = [v for v in (self._depreciation_expense(p) for p in comparative_data) if v is not None]
            dep_values.append(depreciation_expense)
            if len(dep_values) >= 3 and dep_values[0] > 0:
                if dep_values[-1] > dep_values[0] * 1.1:
                    depreciation_trend = TrendDirection.IMPROVING  # rising depreciation charge
                elif dep_values[-1] < dep_values[0] * 0.9:
                    depreciation_trend = TrendDirection.DETERIORATING

        if capex_to_depreciation is None:
            asset_renewal_indicator = "Not assessable (capex or depreciation not reported)"
        elif capex_to_depreciation >= 1.5:
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
            useful_life_estimate=self.note_number(notes, 'average_useful_life'),
            salvage_value_estimate=self.note_number(notes, 'salvage_value'),
            depreciation_trend=depreciation_trend,
            capex_to_depreciation=capex_to_depreciation,
            asset_renewal_indicator=asset_renewal_indicator
        )

    def create_intangible_analysis(self, statements: FinancialStatements) -> IntangibleAssetAnalysis:
        """Intangible asset analysis object ('intangible_assets' excludes goodwill)"""
        bs = statements.balance_sheet
        notes = statements.notes

        identifiable = bs.get('intangible_assets', 0.0)
        goodwill = bs.get('goodwill', 0.0)
        total_assets = bs.get('total_assets')
        total_equity = bs.get('total_equity')

        intangible_intensity = (identifiable + goodwill) / total_assets if total_assets else None
        goodwill_to_equity = goodwill / total_equity if total_equity and total_equity > 0 else None
        goodwill_to_assets = goodwill / total_assets if total_assets else None
        amortization_expense = statements.income_statement.get('amortization')
        weighted_average_life = identifiable / amortization_expense if amortization_expense else None

        impairment_risk_score = 0
        if goodwill_to_equity is not None and goodwill_to_equity > 0.5:
            impairment_risk_score += 30
        if goodwill_to_assets is not None and goodwill_to_assets > 0.2:
            impairment_risk_score += 20
        if intangible_intensity is not None and intangible_intensity > 0.4:
            impairment_risk_score += 15

        return IntangibleAssetAnalysis(
            total_intangibles=identifiable + goodwill,
            identifiable_intangibles=identifiable,
            goodwill=goodwill,
            software=self.note_number(notes, 'software_intangibles', 0.0),
            patents_trademarks=self.note_number(notes, 'patents_trademarks', 0.0),
            customer_relationships=self.note_number(notes, 'customer_relationships', 0.0),
            other_intangibles=self.note_number(notes, 'other_intangibles', 0.0),
            intangible_intensity=intangible_intensity,
            goodwill_to_equity=goodwill_to_equity,
            goodwill_to_assets=goodwill_to_assets,
            amortization_expense=amortization_expense,
            weighted_average_life=weighted_average_life,
            cumulative_impairments=self.note_number(notes, 'cumulative_intangible_impairments', 0.0),
            impairment_risk_score=impairment_risk_score
        )

    def create_capitalization_analysis(self, statements: FinancialStatements) -> CapitalizationAnalysis:
        """Create capitalization vs expensing analysis object"""

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        notes = statements.notes

        # Interest capitalization
        interest_capitalized = self.note_number(notes, 'interest_capitalized', 0.0)
        interest_expensed = income_statement.get('interest_expense', 0)
        total_interest = interest_capitalized + interest_expensed
        interest_cap_ratio = self.safe_divide(interest_capitalized, total_interest) if total_interest > 0 else 0

        # R&D
        rd_capitalized = self.note_number(notes, 'rd_capitalized', 0.0)
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

    def _depreciation_expense(self, statements: FinancialStatements) -> Optional[float]:
        """Depreciation from the income statement, else the cash-flow add-back."""
        dep = statements.income_statement.get('depreciation')
        if dep is None:
            dep = statements.cash_flow.get('depreciation_cf')
        return abs(dep) if dep is not None else None
