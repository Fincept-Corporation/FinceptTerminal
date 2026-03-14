
"""
Financial Statement Multinational Operations Module
========================================

Multinational corporation analysis and foreign operations

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


class CurrencyExposureType(Enum):
    """Types of currency exposure"""
    TRANSACTION = "transaction_exposure"
    TRANSLATION = "translation_exposure"
    ECONOMIC = "economic_exposure"


class TranslationMethod(Enum):
    """Foreign currency translation methods"""
    CURRENT_RATE = "current_rate_method"
    TEMPORAL = "temporal_method"
    HYPERINFLATIONARY = "hyperinflationary_adjustment"


class FunctionalCurrencyDetermination(Enum):
    """Functional currency indicators"""
    LOCAL_CURRENCY = "local_currency"
    PARENT_CURRENCY = "parent_currency"
    THIRD_CURRENCY = "third_currency"


class HyperinflationIndicator(Enum):
    """Hyperinflationary economy indicators"""
    CUMULATIVE_INFLATION = "cumulative_inflation_100_percent"
    CURRENCY_INDEXATION = "widespread_indexation"
    SHORT_TERM_RATES = "high_short_term_interest_rates"
    PRICE_INSTABILITY = "price_level_instability"
    LOCAL_CURRENCY_REJECTION = "local_currency_avoided"


@dataclass
class CurrencyExposureAnalysis:
    """Currency exposure analysis results"""
    total_foreign_exposure: float
    exposure_by_currency: Dict[str, float] = field(default_factory=dict)

    # Transaction exposure
    foreign_receivables: float = 0.0
    foreign_payables: float = 0.0
    net_transaction_exposure: float = 0.0

    # Translation exposure
    net_investment_exposure: float = 0.0
    translation_gains_losses: float = 0.0

    # Risk metrics
    currency_concentration_risk: RiskLevel = RiskLevel.LOW
    hedging_effectiveness: float = 0.0


@dataclass
class TranslationAnalysis:
    """Foreign currency translation analysis"""
    translation_method_used: TranslationMethod
    functional_currencies: List[str] = field(default_factory=list)

    # Translation impacts
    translation_adjustment_oci: float = 0.0
    translation_impact_on_ratios: Dict[str, float] = field(default_factory=dict)

    # Method-specific effects
    current_rate_effects: Dict[str, float] = field(default_factory=dict)
    temporal_method_effects: Dict[str, float] = field(default_factory=dict)

    # Volatility measures
    translation_volatility: float = 0.0
    ratio_stability: RiskLevel = RiskLevel.LOW


@dataclass
class GeographicSegmentAnalysis:
    """Geographic segment performance analysis"""
    segments_by_region: Dict[str, Dict[str, float]] = field(default_factory=dict)

    # Performance metrics by region
    revenue_by_region: Dict[str, float] = field(default_factory=dict)
    profit_by_region: Dict[str, float] = field(default_factory=dict)
    assets_by_region: Dict[str, float] = field(default_factory=dict)

    # Concentration analysis
    geographic_concentration: float = 0.0
    top_region_dependency: float = 0.0

    # Growth analysis
    emerging_markets_exposure: float = 0.0
    developed_markets_exposure: float = 0.0


class MultinationalOperationsAnalyzer(BaseAnalyzer):
    """
    Comprehensive multinational operations analyzer implementing CFA Level II standards.
    Covers currency exposure, translation methods, and geographic analysis.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_multinational_formulas()
        self._initialize_currency_benchmarks()

    def _initialize_multinational_formulas(self):
        """Initialize multinational-specific formulas"""
        self.formula_registry.update({
            'currency_exposure_ratio': lambda foreign_exposure, total_exposure: self.safe_divide(foreign_exposure,
                                                                                                 total_exposure),
            'translation_volatility': lambda translation_std, avg_translation: self.safe_divide(translation_std,
                                                                                                abs(avg_translation)),
            'hedging_ratio': lambda hedged_amount, total_exposure: self.safe_divide(hedged_amount, total_exposure),
            'geographic_concentration': lambda largest_segment, total_revenue: self.safe_divide(largest_segment,
                                                                                                total_revenue),
            'emerging_market_ratio': lambda em_revenue, total_revenue: self.safe_divide(em_revenue, total_revenue),
            'fx_sensitivity': lambda earnings_change, fx_change: self.safe_divide(earnings_change, fx_change)
        })

    def _initialize_currency_benchmarks(self):
        """Initialize currency exposure benchmarks"""
        self.currency_benchmarks = {
            'foreign_exposure_ratio': {'low': 0.2, 'moderate': 0.4, 'high': 0.6, 'very_high': 0.8},
            'currency_concentration': {'diversified': 0.3, 'moderate': 0.5, 'concentrated': 0.7,
                                       'very_concentrated': 0.9},
            'translation_volatility': {'low': 0.1, 'moderate': 0.3, 'high': 0.6, 'very_high': 1.0},
            'emerging_market_exposure': {'low': 0.1, 'moderate': 0.3, 'high': 0.5, 'very_high': 0.7}
        }

        # Hyperinflationary economy thresholds
        self.hyperinflation_thresholds = {
            'cumulative_inflation_3_years': 1.0,  # 100% over 3 years
            'annual_inflation_rate': 0.26,  # 26% annual rate
            'currency_devaluation_annual': 0.5  # 50% annual devaluation
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive multinational operations analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all multinational aspects
        """
        results = []

        # Currency exposure analysis
        results.extend(self._analyze_currency_exposure(statements, comparative_data, industry_data))

        # Translation method analysis
        results.extend(self._analyze_translation_methods(statements, comparative_data))

        # Geographic segment analysis
        results.extend(self._analyze_geographic_segments(statements, comparative_data))

        # Hyperinflationary economies
        results.extend(self._analyze_hyperinflationary_economies(statements, comparative_data))

        # Foreign currency hedging
        results.extend(self._analyze_currency_hedging(statements, comparative_data))

        # Impact on financial ratios
        results.extend(self._analyze_ratio_impacts(statements, comparative_data))

        # Sales sustainability analysis
        results.extend(self._analyze_sales_sustainability(statements, comparative_data))

        return results

    def _analyze_currency_exposure(self, statements: FinancialStatements,
                                   comparative_data: Optional[List[FinancialStatements]] = None,
                                   industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze foreign currency exposure"""
        results = []

        notes = statements.notes
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        # Extract foreign operations data
        foreign_revenue = notes.get('foreign_revenue', 0)
        foreign_assets = notes.get('foreign_assets', 0)
        foreign_receivables = balance_sheet.get('foreign_receivables', 0)
        foreign_payables = balance_sheet.get('foreign_payables', 0)

        total_revenue = income_statement.get('revenue', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        # Foreign Revenue Exposure
        if total_revenue > 0 and foreign_revenue > 0:
            foreign_revenue_ratio = self.safe_divide(foreign_revenue, total_revenue)
            benchmark = self.currency_benchmarks['foreign_exposure_ratio']

            if foreign_revenue_ratio > benchmark['very_high']:
                exposure_interpretation = "Very high foreign revenue exposure - significant currency risk"
                exposure_risk = RiskLevel.HIGH
            elif foreign_revenue_ratio > benchmark['high']:
                exposure_interpretation = "High foreign revenue exposure"
                exposure_risk = RiskLevel.MODERATE
            elif foreign_revenue_ratio > benchmark['moderate']:
                exposure_interpretation = "Moderate foreign revenue exposure"
                exposure_risk = RiskLevel.MODERATE
            else:
                exposure_interpretation = "Low foreign revenue exposure"
                exposure_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Foreign Revenue Exposure",
                value=foreign_revenue_ratio,
                interpretation=exposure_interpretation,
                risk_level=exposure_risk,
                benchmark_comparison=self.compare_to_industry(foreign_revenue_ratio, industry_data.get(
                    'foreign_revenue_ratio') if industry_data else None),
                methodology="Foreign Revenue / Total Revenue",
                limitations=["Currency exposure depends on hedging strategies and natural hedges"]
            ))

        # Foreign Asset Exposure
        if total_assets > 0 and foreign_assets > 0:
            foreign_asset_ratio = self.safe_divide(foreign_assets, total_assets)

            asset_exposure_interpretation = "Significant foreign asset exposure to translation risk" if foreign_asset_ratio > 0.4 else "Moderate foreign asset exposure" if foreign_asset_ratio > 0.2 else "Limited foreign asset exposure"
            asset_exposure_risk = RiskLevel.MODERATE if foreign_asset_ratio > 0.5 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Foreign Asset Exposure",
                value=foreign_asset_ratio,
                interpretation=asset_exposure_interpretation,
                risk_level=asset_exposure_risk,
                methodology="Foreign Assets / Total Assets"
            ))

        # Transaction Exposure Analysis
        if foreign_receivables > 0 or foreign_payables > 0:
            net_transaction_exposure = foreign_receivables - foreign_payables

            if total_assets > 0:
                transaction_exposure_ratio = self.safe_divide(abs(net_transaction_exposure), total_assets)

                transaction_interpretation = f"Net transaction {'asset' if net_transaction_exposure > 0 else 'liability'} exposure of {self.format_percentage(transaction_exposure_ratio)} of total assets"
                transaction_risk = RiskLevel.HIGH if transaction_exposure_ratio > 0.1 else RiskLevel.MODERATE if transaction_exposure_ratio > 0.05 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Net Transaction Exposure",
                    value=transaction_exposure_ratio,
                    interpretation=transaction_interpretation,
                    risk_level=transaction_risk,
                    methodology="|Foreign Receivables - Foreign Payables| / Total Assets"
                ))

        # Currency Concentration Analysis
        currency_exposures = self._extract_currency_exposures(notes)
        if currency_exposures:
            max_currency_exposure = max(currency_exposures.values())
            total_foreign_exposure = sum(currency_exposures.values())

            if total_foreign_exposure > 0:
                currency_concentration = self.safe_divide(max_currency_exposure, total_foreign_exposure)
                concentration_benchmark = self.currency_benchmarks['currency_concentration']

                if currency_concentration > concentration_benchmark['very_concentrated']:
                    concentration_interpretation = "Very high currency concentration risk"
                    concentration_risk = RiskLevel.HIGH
                elif currency_concentration > concentration_benchmark['concentrated']:
                    concentration_interpretation = "High currency concentration"
                    concentration_risk = RiskLevel.MODERATE
                else:
                    concentration_interpretation = "Diversified currency exposure"
                    concentration_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Currency Concentration Risk",
                    value=currency_concentration,
                    interpretation=concentration_interpretation,
                    risk_level=concentration_risk,
                    methodology="Largest Currency Exposure / Total Foreign Exposure"
                ))

        return results

    def _analyze_translation_methods(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze foreign currency translation methods and their impacts"""
        results = []

        notes = statements.notes
        equity_statement = statements.equity_statement
        reporting_standard = statements.company_info.reporting_standard

        # Translation method identification
        translation_method = notes.get('translation_method', 'current_rate')
        functional_currencies = notes.get('functional_currencies', [])

        # Current Rate Method Analysis
        if 'current_rate' in translation_method.lower():
            translation_adjustment = equity_statement.get('translation_adjustment', 0)
            total_equity = statements.balance_sheet.get('total_equity', 0)

            if total_equity > 0 and abs(translation_adjustment) > 0:
                translation_impact = self.safe_divide(abs(translation_adjustment), total_equity)

                impact_interpretation = "Significant translation impact on equity" if translation_impact > 0.1 else "Moderate translation impact" if translation_impact > 0.05 else "Limited translation impact"
                impact_risk = RiskLevel.MODERATE if translation_impact > 0.15 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Current Rate Translation Impact",
                    value=translation_impact,
                    interpretation=impact_interpretation,
                    risk_level=impact_risk,
                    methodology="|Translation Adjustment| / Total Equity",
                    limitations=["Current rate method affects balance sheet but not income statement ratios"]
                ))

        # Temporal Method Analysis
        elif 'temporal' in translation_method.lower():
            fx_gains_losses = statements.income_statement.get('foreign_exchange_gains_losses', 0)
            net_income = statements.income_statement.get('net_income', 0)

            if net_income != 0 and abs(fx_gains_losses) > 0:
                fx_impact_on_earnings = self.safe_divide(abs(fx_gains_losses), abs(net_income))

                earnings_impact_interpretation = "Significant FX impact on earnings under temporal method" if fx_impact_on_earnings > 0.2 else "Moderate FX earnings impact" if fx_impact_on_earnings > 0.1 else "Limited FX earnings impact"
                earnings_impact_risk = RiskLevel.HIGH if fx_impact_on_earnings > 0.3 else RiskLevel.MODERATE if fx_impact_on_earnings > 0.15 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Temporal Method Earnings Impact",
                    value=fx_impact_on_earnings,
                    interpretation=earnings_impact_interpretation,
                    risk_level=earnings_impact_risk,
                    methodology="|FX Gains/Losses| / |Net Income|",
                    limitations=["Temporal method creates income statement volatility"]
                ))

        # Functional Currency Assessment
        if functional_currencies:
            num_functional_currencies = len(functional_currencies)

            functional_currency_interpretation = f"Operations in {num_functional_currencies} functional currencies increases complexity"
            functional_currency_risk = RiskLevel.MODERATE if num_functional_currencies > 5 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Functional Currency Complexity",
                value=num_functional_currencies,
                interpretation=functional_currency_interpretation,
                risk_level=functional_currency_risk,
                methodology="Count of functional currencies used by subsidiaries"
            ))

        # Translation Volatility Analysis
        if comparative_data and len(comparative_data) >= 2:
            translation_adjustments = []
            for past_statements in comparative_data:
                past_adjustment = past_statements.equity_statement.get('translation_adjustment', 0)
                translation_adjustments.append(past_adjustment)

            current_adjustment = equity_statement.get('translation_adjustment', 0)
            translation_adjustments.append(current_adjustment)

            if len(translation_adjustments) > 2:
                translation_volatility = np.std(translation_adjustments)
                mean_adjustment = np.mean([abs(x) for x in translation_adjustments])

                if mean_adjustment > 0:
                    volatility_ratio = self.safe_divide(translation_volatility, mean_adjustment)

                    volatility_interpretation = "High translation volatility" if volatility_ratio > 1.0 else "Moderate translation volatility" if volatility_ratio > 0.5 else "Low translation volatility"
                    volatility_risk = RiskLevel.HIGH if volatility_ratio > 1.5 else RiskLevel.MODERATE if volatility_ratio > 0.8 else RiskLevel.LOW

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name="Translation Volatility",
                        value=volatility_ratio,
                        interpretation=volatility_interpretation,
                        risk_level=volatility_risk,
                        methodology="Standard Deviation of Translation Adjustments / Mean Absolute Adjustment"
                    ))

        return results

    def _analyze_geographic_segments(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze geographic segment performance and concentration"""
        results = []

        notes = statements.notes

        # Extract geographic segment data
        geographic_segments = self._extract_geographic_segments(notes)

        if not geographic_segments:
            return results

        total_revenue = sum(segment.get('revenue', 0) for segment in geographic_segments.values())
        total_assets = sum(segment.get('assets', 0) for segment in geographic_segments.values())

        # Geographic Concentration Analysis
        if total_revenue > 0:
            revenue_by_region = {region: segment.get('revenue', 0) for region, segment in geographic_segments.items()}
            largest_region_revenue = max(revenue_by_region.values())

            geographic_concentration = self.safe_divide(largest_region_revenue, total_revenue)

            concentration_interpretation = "High geographic concentration risk" if geographic_concentration > 0.6 else "Moderate geographic concentration" if geographic_concentration > 0.4 else "Well-diversified geographic presence"
            concentration_risk = RiskLevel.HIGH if geographic_concentration > 0.7 else RiskLevel.MODERATE if geographic_concentration > 0.5 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Geographic Revenue Concentration",
                value=geographic_concentration,
                interpretation=concentration_interpretation,
                risk_level=concentration_risk,
                methodology="Largest Region Revenue / Total Revenue"
            ))

        # Emerging vs Developed Markets Analysis
        emerging_markets = ['china', 'india', 'brazil', 'russia', 'mexico', 'turkey', 'south_africa']
        em_revenue = 0
        dm_revenue = 0

        for region, segment in geographic_segments.items():
            region_revenue = segment.get('revenue', 0)
            if any(em in region.lower() for em in emerging_markets):
                em_revenue += region_revenue
            else:
                dm_revenue += region_revenue

        if total_revenue > 0:
            em_exposure = self.safe_divide(em_revenue, total_revenue)
            benchmark = self.currency_benchmarks['emerging_market_exposure']

            if em_exposure > benchmark['very_high']:
                em_interpretation = "Very high emerging market exposure - significant political and economic risk"
                em_risk = RiskLevel.HIGH
            elif em_exposure > benchmark['high']:
                em_interpretation = "High emerging market exposure"
                em_risk = RiskLevel.MODERATE
            elif em_exposure > benchmark['moderate']:
                em_interpretation = "Moderate emerging market exposure"
                em_risk = RiskLevel.MODERATE
            else:
                em_interpretation = "Low emerging market exposure"
                em_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Emerging Market Exposure",
                value=em_exposure,
                interpretation=em_interpretation,
                risk_level=em_risk,
                methodology="Emerging Market Revenue / Total Revenue"
            ))

        # Regional Performance Analysis
        for region, segment in geographic_segments.items():
            region_revenue = segment.get('revenue', 0)
            region_profit = segment.get('profit', 0)

            if region_revenue > 0:
                region_margin = self.safe_divide(region_profit, region_revenue)
                region_contribution = self.safe_divide(region_revenue, total_revenue) if total_revenue > 0 else 0

                if region_contribution > 0.1:  # Only analyze significant regions
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.PROFITABILITY,
                        metric_name=f"{region.title()} Regional Margin",
                        value=region_margin,
                        interpretation=f"{region.title()} margin of {self.format_percentage(region_margin)} ({self.format_percentage(region_contribution)} of total revenue)",
                        risk_level=RiskLevel.LOW if region_margin > 0.1 else RiskLevel.MODERATE if region_margin > 0.05 else RiskLevel.HIGH,
                        methodology="Regional Profit / Regional Revenue"
                    ))

        return results

    def _analyze_hyperinflationary_economies(self, statements: FinancialStatements,
                                             comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze operations in hyperinflationary economies"""
        results = []

        notes = statements.notes

        # Identify hyperinflationary economies
        hyperinflationary_countries = notes.get('hyperinflationary_countries', [])
        hyperinflationary_revenue = notes.get('hyperinflationary_revenue', 0)
        hyperinflationary_assets = notes.get('hyperinflationary_assets', 0)

        total_revenue = statements.income_statement.get('revenue', 0)
        total_assets = statements.balance_sheet.get('total_assets', 0)

        if hyperinflationary_countries:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Hyperinflationary Economy Operations",
                value=len(hyperinflationary_countries),
                interpretation=f"Operations in {len(hyperinflationary_countries)} hyperinflationary economies: {', '.join(hyperinflationary_countries)}",
                risk_level=RiskLevel.HIGH if len(hyperinflationary_countries) > 2 else RiskLevel.MODERATE,
                methodology="Count and identification of hyperinflationary economy operations",
                limitations=["Hyperinflationary accounting requires complex restatement procedures"]
            ))

        # Hyperinflationary Revenue Exposure
        if hyperinflationary_revenue > 0 and total_revenue > 0:
            hyperinflation_revenue_ratio = self.safe_divide(hyperinflationary_revenue, total_revenue)

            hyperinflation_interpretation = "Significant hyperinflationary economy revenue exposure" if hyperinflation_revenue_ratio > 0.2 else "Moderate hyperinflationary exposure" if hyperinflation_revenue_ratio > 0.1 else "Limited hyperinflationary exposure"
            hyperinflation_risk = RiskLevel.HIGH if hyperinflation_revenue_ratio > 0.3 else RiskLevel.MODERATE if hyperinflation_revenue_ratio > 0.15 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Hyperinflationary Revenue Exposure",
                value=hyperinflation_revenue_ratio,
                interpretation=hyperinflation_interpretation,
                risk_level=hyperinflation_risk,
                methodology="Hyperinflationary Economy Revenue / Total Revenue"
            ))

        # Inflation Adjustment Impact
        inflation_adjustment = notes.get('hyperinflation_adjustment', 0)
        if inflation_adjustment != 0:
            net_income = statements.income_statement.get('net_income', 0)

            if net_income != 0:
                inflation_impact = self.safe_divide(abs(inflation_adjustment), abs(net_income))

                inflation_impact_interpretation = "Significant hyperinflation adjustment impact on earnings" if inflation_impact > 0.2 else "Moderate hyperinflation impact" if inflation_impact > 0.1 else "Limited hyperinflation impact"
                inflation_impact_risk = RiskLevel.HIGH if inflation_impact > 0.3 else RiskLevel.MODERATE if inflation_impact > 0.15 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Hyperinflation Adjustment Impact",
                    value=inflation_impact,
                    interpretation=inflation_impact_interpretation,
                    risk_level=inflation_impact_risk,
                    methodology="|Hyperinflation Adjustment| / |Net Income|"
                ))

        return results

    def _analyze_currency_hedging(self, statements: FinancialStatements,
                                  comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze foreign currency hedging activities"""
        results = []

        notes = statements.notes
        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        # Derivative instruments for hedging
        derivative_assets = balance_sheet.get('derivative_assets', 0)
        derivative_liabilities = balance_sheet.get('derivative_liabilities', 0)

        # Hedging effectiveness
        hedge_ineffectiveness = income_statement.get('hedge_ineffectiveness', 0)

        # Notional amounts of currency derivatives
        fx_derivatives_notional = notes.get('fx_derivatives_notional', 0)
        foreign_exposure_estimate = notes.get('total_foreign_exposure', 0)

        if fx_derivatives_notional > 0:
            # Hedging Ratio Analysis
            if foreign_exposure_estimate > 0:
                hedging_ratio = self.safe_divide(fx_derivatives_notional, foreign_exposure_estimate)

                hedging_interpretation = "High hedging coverage" if hedging_ratio > 0.8 else "Moderate hedging coverage" if hedging_ratio > 0.5 else "Low hedging coverage" if hedging_ratio > 0.2 else "Minimal hedging"
                hedging_risk = RiskLevel.LOW if hedging_ratio > 0.7 else RiskLevel.MODERATE if hedging_ratio > 0.4 else RiskLevel.HIGH

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Currency Hedging Ratio",
                    value=hedging_ratio,
                    interpretation=hedging_interpretation,
                    risk_level=hedging_risk,
                    methodology="FX Derivatives Notional / Estimated Foreign Exposure",
                    limitations=["Hedging effectiveness depends on correlation and timing"]
                ))

        # Hedge Effectiveness Analysis
        if hedge_ineffectiveness != 0:
            net_income = income_statement.get('net_income', 0)

            if net_income != 0:
                ineffectiveness_impact = self.safe_divide(abs(hedge_ineffectiveness), abs(net_income))

                effectiveness_interpretation = "Significant hedge ineffectiveness impacting earnings" if ineffectiveness_impact > 0.05 else "Moderate hedge ineffectiveness" if ineffectiveness_impact > 0.02 else "Good hedge effectiveness"
                effectiveness_risk = RiskLevel.HIGH if ineffectiveness_impact > 0.1 else RiskLevel.MODERATE if ineffectiveness_impact > 0.03 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Hedge Ineffectiveness Impact",
                    value=ineffectiveness_impact,
                    interpretation=effectiveness_interpretation,
                    risk_level=effectiveness_risk,
                    methodology="|Hedge Ineffectiveness| / |Net Income|"
                ))

        return results

    def _analyze_ratio_impacts(self, statements: FinancialStatements,
                               comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze impact of currency fluctuations on financial ratios"""
        results = []

        if not comparative_data or len(comparative_data) == 0:
            return results

        # Calculate key ratios for current and prior periods
        current_ratios = self._calculate_key_ratios(statements)
        prior_ratios = self._calculate_key_ratios(comparative_data[-1])

        # Currency impact assessment
        exchange_rate_changes = statements.notes.get('major_exchange_rate_changes', {})

        if exchange_rate_changes:
            # Analyze ratio stability under currency fluctuations
            for ratio_name, current_value in current_ratios.items():
                prior_value = prior_ratios.get(ratio_name, 0)

                if prior_value != 0:
                    ratio_change = (current_value / prior_value) - 1

                    # Assess if change is primarily due to currency effects
                    if abs(ratio_change) > 0.1:  # 10% threshold
                        currency_impact_interpretation = f"{ratio_name.replace('_', ' ').title()} changed by {self.format_percentage(ratio_change)} - assess currency impact"
                        currency_impact_risk = RiskLevel.MODERATE if abs(ratio_change) > 0.2 else RiskLevel.LOW

                        results.append(AnalysisResult(
                            analysis_type=AnalysisType.QUALITY,
                            metric_name=f"Currency Impact on {ratio_name.replace('_', ' ').title()}",
                            value=ratio_change,
                            interpretation=currency_impact_interpretation,
                            risk_level=currency_impact_risk,
                            methodology="Period-over-period ratio change analysis",
                            limitations=["Ratio changes may be due to operational factors beyond currency"]
                        ))

        return results

    def _analyze_sales_sustainability(self, statements: FinancialStatements,
                                      comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze sustainability of sales growth components"""
        results = []

        if not comparative_data or len(comparative_data) == 0:
            return results

        notes = statements.notes
        income_statement = statements.income_statement

        current_revenue = income_statement.get('revenue', 0)
        prior_revenue = comparative_data[-1].income_statement.get('revenue', 0)

        if prior_revenue > 0:
            total_growth = (current_revenue / prior_revenue) - 1

            # Decompose sales growth
            organic_growth = notes.get('organic_sales_growth', 0)
            fx_impact_on_sales = notes.get('fx_impact_on_sales', 0)
            acquisition_impact = notes.get('acquisition_impact_on_sales', 0)

            # Volume vs Price analysis
            volume_growth = notes.get('volume_growth', 0)
            price_growth = notes.get('price_growth', 0)

            # Analyze growth sustainability
            if organic_growth != 0:
                organic_ratio = self.safe_divide(organic_growth, total_growth) if total_growth != 0 else 0

                sustainability_interpretation = "Sustainable organic growth drives revenue" if organic_ratio > 0.7 else "Mixed growth drivers" if organic_ratio > 0.4 else "Growth heavily dependent on external factors"
                sustainability_risk = RiskLevel.LOW if organic_ratio > 0.6 else RiskLevel.MODERATE if organic_ratio > 0.3 else RiskLevel.HIGH

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Organic Growth Sustainability",
                    value=organic_ratio,
                    interpretation=sustainability_interpretation,
                    risk_level=sustainability_risk,
                    methodology="Organic Growth / Total Revenue Growth"
                ))

            # FX Impact on Growth
            if fx_impact_on_sales != 0 and total_growth != 0:
                fx_contribution = self.safe_divide(fx_impact_on_sales, total_growth)

                fx_interpretation = f"Currency {'tailwind' if fx_impact_on_sales > 0 else 'headwind'} contributed {self.format_percentage(abs(fx_contribution))} to revenue growth"
                fx_risk = RiskLevel.MODERATE if abs(fx_contribution) > 0.3 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="FX Impact on Revenue Growth",
                    value=fx_contribution,
                    interpretation=fx_interpretation,
                    risk_level=fx_risk,
                    methodology="FX Impact on Sales / Total Revenue Growth"
                ))

        return results

    def _extract_currency_exposures(self, notes: Dict) -> Dict[str, float]:
        """Extract currency exposure data from notes"""
        currency_exposures = {}

        # Look for currency-specific exposures
        for key, value in notes.items():
            if 'currency' in key.lower() and isinstance(value, (int, float)):
                currency_name = key.replace('_currency_exposure', '').replace('_exposure', '')
                currency_exposures[currency_name] = value

        return currency_exposures

    def _extract_geographic_segments(self, notes: Dict) -> Dict[str, Dict[str, float]]:
        """Extract geographic segment data from notes"""
        segments = {}

        # Common geographic regions
        regions = ['north_america', 'europe', 'asia_pacific', 'latin_america', 'middle_east_africa']

        for region in regions:
            segment_data = {}
            for metric in ['revenue', 'profit', 'assets']:
                key = f"{region}_{metric}"
                if key in notes:
                    segment_data[metric] = notes[key]

            if segment_data:
                segments[region] = segment_data

        return segments

    def _calculate_key_ratios(self, statements: FinancialStatements) -> Dict[str, float]:
        """Calculate key financial ratios for currency impact analysis"""
        ratios = {}

        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        revenue = income_statement.get('revenue', 0)
        net_income = income_statement.get('net_income', 0)
        total_assets = balance_sheet.get('total_assets', 0)
        total_equity = balance_sheet.get('total_equity', 0)

        if revenue > 0:
            ratios['net_margin'] = self.safe_divide(net_income, revenue)

        if total_assets > 0:
            ratios['asset_turnover'] = self.safe_divide(revenue, total_assets)
            ratios['roa'] = self.safe_divide(net_income, total_assets)

        if total_equity > 0:
            ratios['roe'] = self.safe_divide(net_income, total_equity)

        return ratios

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key multinational operations metrics"""

        notes = statements.notes
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        metrics = {}

        # Foreign exposure ratios
        foreign_revenue = notes.get('foreign_revenue', 0)
        foreign_assets = notes.get('foreign_assets', 0)
        total_revenue = income_statement.get('revenue', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        if total_revenue > 0:
            metrics['foreign_revenue_ratio'] = self.safe_divide(foreign_revenue, total_revenue)

        if total_assets > 0:
            metrics['foreign_asset_ratio'] = self.safe_divide(foreign_assets, total_assets)

        # Translation impact
        translation_adjustment = statements.equity_statement.get('translation_adjustment', 0)
        total_equity = balance_sheet.get('total_equity', 0)

        if total_equity > 0:
            metrics['translation_impact_ratio'] = self.safe_divide(abs(translation_adjustment), total_equity)

        # Hedging metrics
        fx_derivatives_notional = notes.get('fx_derivatives_notional', 0)
        total_foreign_exposure = notes.get('total_foreign_exposure', 0)

        if total_foreign_exposure > 0:
            metrics['hedging_ratio'] = self.safe_divide(fx_derivatives_notional, total_foreign_exposure)

        # Geographic concentration
        geographic_segments = self._extract_geographic_segments(notes)
        if geographic_segments:
            total_segment_revenue = sum(segment.get('revenue', 0) for segment in geographic_segments.values())
            if total_segment_revenue > 0:
                max_segment_revenue = max(segment.get('revenue', 0) for segment in geographic_segments.values())
                metrics['geographic_concentration'] = self.safe_divide(max_segment_revenue, total_segment_revenue)

        return metrics

    def create_currency_exposure_analysis(self, statements: FinancialStatements) -> CurrencyExposureAnalysis:
        """Create comprehensive currency exposure analysis object"""

        notes = statements.notes
        balance_sheet = statements.balance_sheet

        # Extract exposure data
        foreign_revenue = notes.get('foreign_revenue', 0)
        foreign_assets = notes.get('foreign_assets', 0)
        foreign_receivables = balance_sheet.get('foreign_receivables', 0)
        foreign_payables = balance_sheet.get('foreign_payables', 0)

        total_foreign_exposure = foreign_revenue + foreign_assets
        exposure_by_currency = self._extract_currency_exposures(notes)

        # Transaction exposure
        net_transaction_exposure = foreign_receivables - foreign_payables

        # Translation exposure
        net_investment_exposure = foreign_assets
        translation_gains_losses = statements.income_statement.get('foreign_exchange_gains_losses', 0)

        # Risk assessment
        currency_exposures = list(exposure_by_currency.values()) if exposure_by_currency else [total_foreign_exposure]
        max_exposure = max(currency_exposures) if currency_exposures else 0
        total_exposure = sum(currency_exposures) if currency_exposures else total_foreign_exposure

        concentration_ratio = self.safe_divide(max_exposure, total_exposure) if total_exposure > 0 else 0

        if concentration_ratio > 0.7:
            currency_concentration_risk = RiskLevel.HIGH
        elif concentration_ratio > 0.5:
            currency_concentration_risk = RiskLevel.MODERATE
        else:
            currency_concentration_risk = RiskLevel.LOW

        # Hedging effectiveness
        fx_derivatives_notional = notes.get('fx_derivatives_notional', 0)
        hedging_effectiveness = self.safe_divide(fx_derivatives_notional,
                                                 total_foreign_exposure) if total_foreign_exposure > 0 else 0

        return CurrencyExposureAnalysis(
            total_foreign_exposure=total_foreign_exposure,
            exposure_by_currency=exposure_by_currency,
            foreign_receivables=foreign_receivables,
            foreign_payables=foreign_payables,
            net_transaction_exposure=net_transaction_exposure,
            net_investment_exposure=net_investment_exposure,
            translation_gains_losses=translation_gains_losses,
            currency_concentration_risk=currency_concentration_risk,
            hedging_effectiveness=hedging_effectiveness
        )

    def create_geographic_analysis(self, statements: FinancialStatements) -> GeographicSegmentAnalysis:
        """Create comprehensive geographic segment analysis object"""

        notes = statements.notes

        segments_by_region = self._extract_geographic_segments(notes)

        # Extract performance metrics by region
        revenue_by_region = {}
        profit_by_region = {}
        assets_by_region = {}

        for region, segment in segments_by_region.items():
            revenue_by_region[region] = segment.get('revenue', 0)
            profit_by_region[region] = segment.get('profit', 0)
            assets_by_region[region] = segment.get('assets', 0)

        # Calculate concentration metrics
        total_revenue = sum(revenue_by_region.values())
        max_region_revenue = max(revenue_by_region.values()) if revenue_by_region else 0

        geographic_concentration = self.safe_divide(max_region_revenue, total_revenue) if total_revenue > 0 else 0
        top_region_dependency = geographic_concentration

        # Emerging vs developed markets
        emerging_markets = ['china', 'india', 'brazil', 'russia', 'mexico', 'turkey', 'south_africa']
        emerging_revenue = 0
        developed_revenue = 0

        for region, revenue in revenue_by_region.items():
            if any(em in region.lower() for em in emerging_markets):
                emerging_revenue += revenue
            else:
                developed_revenue += revenue

        emerging_markets_exposure = self.safe_divide(emerging_revenue, total_revenue) if total_revenue > 0 else 0
        developed_markets_exposure = self.safe_divide(developed_revenue, total_revenue) if total_revenue > 0 else 0

        return GeographicSegmentAnalysis(
            segments_by_region=segments_by_region,
            revenue_by_region=revenue_by_region,
            profit_by_region=profit_by_region,
            assets_by_region=assets_by_region,
            geographic_concentration=geographic_concentration,
            top_region_dependency=top_region_dependency,
            emerging_markets_exposure=emerging_markets_exposure,
            developed_markets_exposure=developed_markets_exposure
        )