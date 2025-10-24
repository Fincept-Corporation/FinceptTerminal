
"""
Financial Statement Intercorporate Investments Module
========================================

Intercorporate investment analysis and accounting treatment

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


class InvestmentClassification(Enum):
    """Investment classification under IFRS/GAAP"""
    FVTPL = "fair_value_through_profit_loss"  # Fair Value Through P&L
    FVOCI = "fair_value_through_oci"  # Fair Value Through OCI
    AMORTIZED_COST = "amortized_cost"
    EQUITY_METHOD = "equity_method"
    CONSOLIDATION = "consolidation"


class ControlLevel(Enum):
    """Level of control/influence"""
    NO_INFLUENCE = "no_influence"  # < 20%
    SIGNIFICANT_INFLUENCE = "significant_influence"  # 20-50%
    CONTROL = "control"  # > 50%
    JOINT_CONTROL = "joint_control"


class BusinessCombinationType(Enum):
    """Types of business combinations"""
    ACQUISITION = "acquisition"
    MERGER = "merger"
    ASSET_PURCHASE = "asset_purchase"
    REVERSE_ACQUISITION = "reverse_acquisition"


@dataclass
class FinancialAssetAnalysis:
    """Analysis of financial asset investments"""
    total_financial_assets: float
    classification_breakdown: Dict[str, float] = field(default_factory=dict)

    # Valuation metrics
    fair_value_assets: float = 0.0
    amortized_cost_assets: float = 0.0
    unrealized_gains_losses: float = 0.0

    # Risk assessment
    credit_risk_exposure: float = 0.0
    market_risk_exposure: float = 0.0
    liquidity_risk: RiskLevel = RiskLevel.LOW

    # Performance metrics
    investment_returns: float = 0.0
    dividend_income: float = 0.0


@dataclass
class AssociateAnalysis:
    """Analysis of investments in associates"""
    total_associate_investments: float
    number_of_associates: int

    # Equity method metrics
    share_of_profits_losses: float
    dividend_income_associates: float
    carrying_value_change: float

    # Performance assessment
    associate_roe: float = 0.0
    associate_performance_trend: TrendDirection = TrendDirection.STABLE
    impairment_indicators: List[str] = field(default_factory=list)


@dataclass
class JointVentureAnalysis:
    """Analysis of joint venture investments"""
    total_jv_investments: float
    number_of_joint_ventures: int

    # Joint control assessment
    control_structure: str = ""
    governance_quality: float = 0.0

    # Financial performance
    jv_contribution_to_earnings: float = 0.0
    jv_strategic_value: str = ""
    exit_strategy_clarity: RiskLevel = RiskLevel.MODERATE


@dataclass
class BusinessCombinationAnalysis:
    """Analysis of business combinations"""
    acquisition_date: str
    purchase_price: float
    fair_value_acquired_assets: float
    assumed_liabilities: float

    # Goodwill analysis
    goodwill_recognized: float
    bargain_purchase_gain: float = 0.0

    # Integration assessment
    synergies_realized: float = 0.0
    integration_costs: float = 0.0
    performance_vs_expectations: str = ""


class IntercorporateInvestmentsAnalyzer(BaseAnalyzer):
    """
    Comprehensive intercorporate investments analyzer implementing CFA Level II standards.
    Covers all types of intercorporate investments and their accounting treatments.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_investment_formulas()
        self._initialize_investment_benchmarks()

    def _initialize_investment_formulas(self):
        """Initialize investment-specific formulas"""
        self.formula_registry.update({
            'investment_return': lambda gains_income, beginning_value: self.safe_divide(gains_income, beginning_value),
            'dividend_yield': lambda dividend_income, investment_value: self.safe_divide(dividend_income,
                                                                                         investment_value),
            'investment_intensity': lambda total_investments, total_assets: self.safe_divide(total_investments,
                                                                                             total_assets),
            'equity_method_return': lambda share_of_earnings, carrying_value: self.safe_divide(share_of_earnings,
                                                                                               carrying_value),
            'goodwill_premium': lambda goodwill, purchase_price: self.safe_divide(goodwill, purchase_price),
            'acquisition_multiple': lambda purchase_price, acquired_earnings: self.safe_divide(purchase_price,
                                                                                               acquired_earnings)
        })

    def _initialize_investment_benchmarks(self):
        """Initialize investment-specific benchmarks"""
        self.investment_benchmarks = {
            'investment_intensity': {'low': 0.1, 'moderate': 0.2, 'high': 0.4, 'very_high': 0.6},
            'investment_return': {'excellent': 0.15, 'good': 0.10, 'adequate': 0.05, 'poor': 0.0},
            'goodwill_premium': {'reasonable': 0.3, 'high': 0.5, 'excessive': 0.7},
            'acquisition_multiple': {'reasonable': 15, 'high': 25, 'excessive': 40}
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive intercorporate investments analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all investment aspects
        """
        results = []

        # Financial assets analysis
        results.extend(self._analyze_financial_assets(statements, comparative_data, industry_data))

        # Associates analysis
        results.extend(self._analyze_associates(statements, comparative_data))

        # Joint ventures analysis
        results.extend(self._analyze_joint_ventures(statements, comparative_data))

        # Business combinations analysis
        results.extend(self._analyze_business_combinations(statements, comparative_data))

        # Special purpose entities analysis
        results.extend(self._analyze_spe_vie(statements, comparative_data))

        # IFRS vs US GAAP differences
        results.extend(self._analyze_gaap_differences(statements))

        # Overall investment strategy assessment
        results.extend(self._assess_investment_strategy(statements, comparative_data))

        return results

    def _analyze_financial_assets(self, statements: FinancialStatements,
                                  comparative_data: Optional[List[FinancialStatements]] = None,
                                  industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze financial asset investments"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        # Extract financial asset categories
        financial_assets = {
            'trading_securities': balance_sheet.get('trading_securities', 0),
            'afs_securities': balance_sheet.get('available_for_sale_securities', 0),
            'htm_securities': balance_sheet.get('held_to_maturity_securities', 0),
            'fvtpl_assets': balance_sheet.get('fvtpl_financial_assets', 0),
            'fvoci_assets': balance_sheet.get('fvoci_financial_assets', 0),
            'amortized_cost_assets': balance_sheet.get('amortized_cost_financial_assets', 0)
        }

        total_financial_assets = sum(financial_assets.values())
        total_assets = balance_sheet.get('total_assets', 0)

        if total_financial_assets <= 0:
            return results

        # Investment Intensity Analysis
        if total_assets > 0:
            investment_intensity = self.safe_divide(total_financial_assets, total_assets)
            benchmark = self.investment_benchmarks['investment_intensity']

            if investment_intensity > benchmark['very_high']:
                intensity_interpretation = "Very high financial asset intensity - investment-focused business model"
                intensity_risk = RiskLevel.MODERATE
            elif investment_intensity > benchmark['high']:
                intensity_interpretation = "High financial asset concentration"
                intensity_risk = RiskLevel.MODERATE
            elif investment_intensity > benchmark['moderate']:
                intensity_interpretation = "Moderate financial asset investments"
                intensity_risk = RiskLevel.LOW
            else:
                intensity_interpretation = "Low financial asset concentration"
                intensity_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Financial Asset Intensity",
                value=investment_intensity,
                interpretation=intensity_interpretation,
                risk_level=intensity_risk,
                benchmark_comparison=self.compare_to_industry(investment_intensity, industry_data.get(
                    'investment_intensity') if industry_data else None),
                methodology="Total Financial Assets / Total Assets"
            ))

        # Classification Analysis
        if total_financial_assets > 0:
            for asset_type, value in financial_assets.items():
                if value > 0:
                    asset_ratio = self.safe_divide(value, total_financial_assets)
                    asset_name = asset_type.replace('_', ' ').title()

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name=f"{asset_name} Composition",
                        value=asset_ratio,
                        interpretation=f"{asset_name} represents {self.format_percentage(asset_ratio)} of total financial assets",
                        risk_level=RiskLevel.LOW,
                        methodology=f"{asset_name} / Total Financial Assets"
                    ))

        # Fair Value vs Amortized Cost Analysis
        fair_value_assets = financial_assets['trading_securities'] + financial_assets['afs_securities'] + \
                            financial_assets['fvtpl_assets'] + financial_assets['fvoci_assets']
        amortized_cost_assets = financial_assets['htm_securities'] + financial_assets['amortized_cost_assets']

        if total_financial_assets > 0:
            fair_value_ratio = self.safe_divide(fair_value_assets, total_financial_assets)

            fv_interpretation = "High fair value measurement exposure - significant market risk" if fair_value_ratio > 0.7 else "Moderate fair value exposure" if fair_value_ratio > 0.4 else "Low fair value measurement exposure"
            fv_risk = RiskLevel.HIGH if fair_value_ratio > 0.8 else RiskLevel.MODERATE if fair_value_ratio > 0.5 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Fair Value Asset Ratio",
                value=fair_value_ratio,
                interpretation=fv_interpretation,
                risk_level=fv_risk,
                methodology="Fair Value Assets / Total Financial Assets",
                limitations=["Fair value assets subject to market volatility"]
            ))

        # Investment Performance Analysis
        investment_income = income_statement.get('investment_income', 0)
        realized_gains = income_statement.get('realized_investment_gains', 0)
        unrealized_gains = income_statement.get('unrealized_investment_gains', 0)

        total_investment_return = investment_income + realized_gains + unrealized_gains

        if total_financial_assets > 0 and total_investment_return != 0:
            investment_return = self.safe_divide(total_investment_return, total_financial_assets)
            benchmark = self.investment_benchmarks['investment_return']

            return_risk = self.assess_risk_level(investment_return, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Financial Asset Return",
                value=investment_return,
                interpretation=self.generate_interpretation("financial asset return", investment_return, return_risk,
                                                            AnalysisType.PROFITABILITY),
                risk_level=return_risk,
                methodology="(Investment Income + Realized Gains + Unrealized Gains) / Total Financial Assets"
            ))

        return results

    def _analyze_associates(self, statements: FinancialStatements,
                            comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze investments in associates using equity method"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        associate_investments = balance_sheet.get('investments_in_associates', 0)
        share_of_associate_profits = income_statement.get('share_of_associate_profits', 0)
        associate_dividends = income_statement.get('dividends_from_associates', 0)

        if associate_investments <= 0:
            return results

        # Associate Investment Analysis
        total_assets = balance_sheet.get('total_assets', 0)
        if total_assets > 0:
            associate_intensity = self.safe_divide(associate_investments, total_assets)

            associate_interpretation = "Significant associate investments - substantial equity method exposure" if associate_intensity > 0.2 else "Moderate associate investments" if associate_intensity > 0.1 else "Limited associate investments"
            associate_risk = RiskLevel.MODERATE if associate_intensity > 0.3 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Associate Investment Intensity",
                value=associate_intensity,
                interpretation=associate_interpretation,
                risk_level=associate_risk,
                methodology="Investments in Associates / Total Assets"
            ))

        # Equity Method Return Analysis
        if associate_investments > 0:
            equity_method_return = self.safe_divide(share_of_associate_profits, associate_investments)

            if equity_method_return > 0.15:
                return_interpretation = "Strong associate performance contributing to earnings"
                return_risk = RiskLevel.LOW
            elif equity_method_return > 0.05:
                return_interpretation = "Adequate associate performance"
                return_risk = RiskLevel.LOW
            elif equity_method_return > 0:
                return_interpretation = "Weak associate performance"
                return_risk = RiskLevel.MODERATE
            else:
                return_interpretation = "Associates generating losses - potential impairment concern"
                return_risk = RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Equity Method Return",
                value=equity_method_return,
                interpretation=return_interpretation,
                risk_level=return_risk,
                methodology="Share of Associate Profits / Investment in Associates"
            ))

        # Dividend Payout from Associates
        if share_of_associate_profits > 0 and associate_dividends > 0:
            associate_payout_ratio = self.safe_divide(associate_dividends, share_of_associate_profits)

            payout_interpretation = "High dividend payout from associates - good cash generation" if associate_payout_ratio > 0.6 else "Moderate dividend payout" if associate_payout_ratio > 0.3 else "Low dividend payout - associates retaining earnings for growth"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Associate Dividend Payout Ratio",
                value=associate_payout_ratio,
                interpretation=payout_interpretation,
                risk_level=RiskLevel.LOW,
                methodology="Dividends from Associates / Share of Associate Profits"
            ))

        # Associate Investment Trend Analysis
        if comparative_data and len(comparative_data) > 0:
            prev_associate_investments = comparative_data[-1].balance_sheet.get('investments_in_associates', 0)

            if prev_associate_investments > 0:
                associate_growth = (associate_investments / prev_associate_investments) - 1

                growth_interpretation = "Significant expansion in associate investments" if associate_growth > 0.2 else "Moderate growth in associate investments" if associate_growth > 0.05 else "Stable associate investment base" if associate_growth > -0.05 else "Declining associate investments"
                growth_risk = RiskLevel.MODERATE if associate_growth > 0.5 or associate_growth < -0.2 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name="Associate Investment Growth",
                    value=associate_growth,
                    interpretation=growth_interpretation,
                    risk_level=growth_risk,
                    methodology="(Current Associate Investments - Prior Associate Investments) / Prior Associate Investments"
                ))

        return results

    def _analyze_joint_ventures(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze joint venture investments"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        joint_venture_investments = balance_sheet.get('investments_in_joint_ventures', 0)
        share_of_jv_profits = income_statement.get('share_of_jv_profits', 0)

        if joint_venture_investments <= 0:
            return results

        # Joint Venture Investment Analysis
        total_assets = balance_sheet.get('total_assets', 0)
        if total_assets > 0:
            jv_intensity = self.safe_divide(joint_venture_investments, total_assets)

            jv_interpretation = "Significant joint venture exposure" if jv_intensity > 0.15 else "Moderate joint venture investments" if jv_intensity > 0.05 else "Limited joint venture exposure"
            jv_risk = RiskLevel.MODERATE if jv_intensity > 0.25 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Joint Venture Investment Intensity",
                value=jv_intensity,
                interpretation=jv_interpretation,
                risk_level=jv_risk,
                methodology="Investments in Joint Ventures / Total Assets",
                limitations=["Joint ventures involve shared control and coordination challenges"]
            ))

        # Joint Venture Performance
        if joint_venture_investments > 0:
            jv_return = self.safe_divide(share_of_jv_profits, joint_venture_investments)

            jv_performance_interpretation = "Strong joint venture performance" if jv_return > 0.12 else "Adequate joint venture performance" if jv_return > 0.06 else "Weak joint venture performance" if jv_return > 0 else "Joint ventures generating losses"
            jv_performance_risk = RiskLevel.LOW if jv_return > 0.08 else RiskLevel.MODERATE if jv_return > 0 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Joint Venture Return",
                value=jv_return,
                interpretation=jv_performance_interpretation,
                risk_level=jv_performance_risk,
                methodology="Share of JV Profits / Investment in Joint Ventures"
            ))

        # Joint Venture Strategy Assessment
        jv_count = notes.get('number_of_joint_ventures', 0)
        if jv_count > 0:
            avg_jv_size = self.safe_divide(joint_venture_investments, jv_count)

            strategy_interpretation = f"Portfolio of {jv_count} joint ventures with average investment of ${avg_jv_size:,.0f}"
            strategy_risk = RiskLevel.MODERATE if jv_count > 10 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Joint Venture Portfolio",
                value=avg_jv_size,
                interpretation=strategy_interpretation,
                risk_level=strategy_risk,
                methodology="Total JV Investments / Number of Joint Ventures"
            ))

        return results

    def _analyze_business_combinations(self, statements: FinancialStatements,
                                       comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze business combinations and acquisitions"""
        results = []

        balance_sheet = statements.balance_sheet
        cash_flow = statements.cash_flow
        notes = statements.notes

        acquisitions = cash_flow.get('acquisitions', 0)
        goodwill = balance_sheet.get('goodwill', 0)

        # Acquisition Activity Analysis
        if acquisitions > 0:
            total_assets = balance_sheet.get('total_assets', 0)

            if total_assets > 0:
                acquisition_intensity = self.safe_divide(acquisitions, total_assets)

                acquisition_interpretation = "Significant acquisition activity" if acquisition_intensity > 0.1 else "Moderate acquisition activity" if acquisition_intensity > 0.05 else "Limited acquisition activity"
                acquisition_risk = RiskLevel.MODERATE if acquisition_intensity > 0.2 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name="Acquisition Activity Intensity",
                    value=acquisition_intensity,
                    interpretation=acquisition_interpretation,
                    risk_level=acquisition_risk,
                    methodology="Cash Paid for Acquisitions / Total Assets"
                ))

        # Goodwill Analysis from Acquisitions
        if goodwill > 0 and acquisitions > 0:
            # Estimate goodwill premium (simplified - assumes current year goodwill from acquisitions)
            goodwill_premium = self.safe_divide(goodwill,
                                                acquisitions) if acquisitions > goodwill else self.safe_divide(goodwill,
                                                                                                               goodwill + acquisitions)
            benchmark = self.investment_benchmarks['goodwill_premium']

            if goodwill_premium > benchmark['excessive']:
                premium_interpretation = "Very high goodwill premium - potential overpayment concern"
                premium_risk = RiskLevel.HIGH
            elif goodwill_premium > benchmark['high']:
                premium_interpretation = "High goodwill premium - monitor integration success"
                premium_risk = RiskLevel.MODERATE
            elif goodwill_premium > benchmark['reasonable']:
                premium_interpretation = "Reasonable goodwill premium"
                premium_risk = RiskLevel.LOW
            else:
                premium_interpretation = "Low goodwill premium - strategic acquisition"
                premium_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Acquisition Goodwill Premium",
                value=goodwill_premium,
                interpretation=premium_interpretation,
                risk_level=premium_risk,
                methodology="Goodwill / Acquisition Price (simplified)",
                limitations=["Simplified calculation - detailed purchase price allocation needed for precision"]
            ))

        # Acquisition Integration Assessment
        if comparative_data and len(comparative_data) >= 2:
            # Look for patterns in acquisition activity
            acquisition_history = []
            for past_statements in comparative_data:
                past_acquisitions = past_statements.cash_flow.get('acquisitions', 0)
                acquisition_history.append(past_acquisitions)

            acquisition_history.append(acquisitions)

            total_acquisitions = sum(acquisition_history)
            if total_acquisitions > 0:
                acquisition_frequency = sum(1 for x in acquisition_history if x > 0) / len(acquisition_history)

                frequency_interpretation = "Frequent acquirer - high integration complexity" if acquisition_frequency > 0.7 else "Moderate acquisition frequency" if acquisition_frequency > 0.3 else "Infrequent acquisitions"
                frequency_risk = RiskLevel.HIGH if acquisition_frequency > 0.8 else RiskLevel.MODERATE if acquisition_frequency > 0.5 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Acquisition Frequency",
                    value=acquisition_frequency,
                    interpretation=frequency_interpretation,
                    risk_level=frequency_risk,
                    methodology="Periods with Acquisitions / Total Periods"
                ))

        return results

    def _analyze_spe_vie(self, statements: FinancialStatements,
                         comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze Special Purpose Entities and Variable Interest Entities"""
        results = []

        notes = statements.notes

        # VIE Analysis
        vie_assets = notes.get('vie_consolidated_assets', 0)
        vie_liabilities = notes.get('vie_consolidated_liabilities', 0)
        unconsolidated_vie_exposure = notes.get('unconsolidated_vie_exposure', 0)

        total_assets = statements.balance_sheet.get('total_assets', 0)

        # Consolidated VIE Analysis
        if vie_assets > 0 and total_assets > 0:
            vie_asset_ratio = self.safe_divide(vie_assets, total_assets)

            vie_interpretation = "Significant VIE consolidation impact" if vie_asset_ratio > 0.2 else "Moderate VIE consolidation" if vie_asset_ratio > 0.1 else "Limited VIE consolidation"
            vie_risk = RiskLevel.MODERATE if vie_asset_ratio > 0.3 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="VIE Asset Consolidation Ratio",
                value=vie_asset_ratio,
                interpretation=vie_interpretation,
                risk_level=vie_risk,
                methodology="VIE Consolidated Assets / Total Assets",
                limitations=["VIE consolidation may obscure underlying business performance"]
            ))

        # Unconsolidated VIE Exposure
        if unconsolidated_vie_exposure > 0:
            if total_assets > 0:
                vie_exposure_ratio = self.safe_divide(unconsolidated_vie_exposure, total_assets)

                exposure_interpretation = "Significant off-balance sheet VIE exposure" if vie_exposure_ratio > 0.1 else "Moderate off-balance sheet exposure" if vie_exposure_ratio > 0.05 else "Limited off-balance sheet VIE exposure"
                exposure_risk = RiskLevel.HIGH if vie_exposure_ratio > 0.2 else RiskLevel.MODERATE if vie_exposure_ratio > 0.1 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Unconsolidated VIE Exposure",
                    value=vie_exposure_ratio,
                    interpretation=exposure_interpretation,
                    risk_level=exposure_risk,
                    methodology="Unconsolidated VIE Exposure / Total Assets",
                    limitations=["Off-balance sheet exposures may represent hidden risks"]
                ))

        # SPE Disclosure Quality
        spe_disclosures = sum(1 for key in notes.keys() if
                              'spe' in key.lower() or 'vie' in key.lower() or 'special_purpose' in key.lower())

        if spe_disclosures > 0:
            disclosure_interpretation = "Comprehensive SPE/VIE disclosures" if spe_disclosures > 5 else "Adequate SPE/VIE disclosures" if spe_disclosures > 2 else "Limited SPE/VIE disclosures"
            disclosure_risk = RiskLevel.LOW if spe_disclosures > 3 else RiskLevel.MODERATE if spe_disclosures > 1 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="SPE/VIE Disclosure Quality",
                value=spe_disclosures,
                interpretation=disclosure_interpretation,
                risk_level=disclosure_risk,
                methodology="Count of SPE/VIE related disclosures"
            ))

        return results

    def _analyze_gaap_differences(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Analyze IFRS vs US GAAP differences in intercorporate investments"""
        results = []

        reporting_standard = statements.company_info.reporting_standard
        notes = statements.notes

        # Classification and Measurement Differences
        if reporting_standard == ReportingStandard.IFRS:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="IFRS Investment Classification",
                value=1.0,
                interpretation="Under IFRS 9, financial assets classified based on business model and cash flow characteristics",
                risk_level=RiskLevel.LOW,
                methodology="IFRS 9 classification assessment",
                limitations=["IFRS allows more judgment in classification decisions"]
            ))

        elif reporting_standard == ReportingStandard.US_GAAP:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="US GAAP Investment Classification",
                value=1.0,
                interpretation="Under US GAAP, financial assets follow traditional held-to-maturity, available-for-sale, and trading classifications",
                risk_level=RiskLevel.LOW,
                methodology="US GAAP classification assessment",
                limitations=["US GAAP has more prescriptive classification rules"]
            ))

        # Joint Venture Accounting Differences
        joint_ventures = statements.balance_sheet.get('investments_in_joint_ventures', 0)

        if joint_ventures > 0:
            if reporting_standard == ReportingStandard.IFRS:
                jv_interpretation = "Under IFRS 11, joint ventures must use equity method (proportionate consolidation eliminated)"
            else:
                jv_interpretation = "Under US GAAP, joint ventures typically use equity method"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Joint Venture Accounting Method",
                value=1.0,
                interpretation=jv_interpretation,
                risk_level=RiskLevel.LOW,
                methodology="Assessment of joint venture accounting standards"
            ))

        return results

    def _assess_investment_strategy(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Assess overall intercorporate investment strategy"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        # Total Investment Portfolio Analysis
        total_investments = (
                balance_sheet.get('trading_securities', 0) +
                balance_sheet.get('available_for_sale_securities', 0) +
                balance_sheet.get('held_to_maturity_securities', 0) +
                balance_sheet.get('investments_in_associates', 0) +
                balance_sheet.get('investments_in_joint_ventures', 0)
        )

        total_assets = balance_sheet.get('total_assets', 0)

        if total_investments > 0 and total_assets > 0:
            total_investment_ratio = self.safe_divide(total_investments, total_assets)

            strategy_interpretation = "Investment-intensive business model" if total_investment_ratio > 0.4 else "Moderate investment strategy" if total_investment_ratio > 0.2 else "Limited investment focus"
            strategy_risk = RiskLevel.MODERATE if total_investment_ratio > 0.5 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Total Investment Intensity",
                value=total_investment_ratio,
                interpretation=strategy_interpretation,
                risk_level=strategy_risk,
                methodology="Total Intercorporate Investments / Total Assets"
            ))

        # Investment Diversification Analysis
        investment_types = {
            'Financial Assets': balance_sheet.get('trading_securities', 0) + balance_sheet.get(
                'available_for_sale_securities', 0),
            'Associates': balance_sheet.get('investments_in_associates', 0),
            'Joint Ventures': balance_sheet.get('investments_in_joint_ventures', 0)
        }

        non_zero_types = sum(1 for value in investment_types.values() if value > 0)

        if non_zero_types > 0:
            diversification_interpretation = "Well-diversified investment portfolio" if non_zero_types >= 3 else "Moderately diversified investments" if non_zero_types == 2 else "Concentrated investment strategy"
            diversification_risk = RiskLevel.LOW if non_zero_types >= 3 else RiskLevel.MODERATE if non_zero_types == 2 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Investment Diversification",
                value=non_zero_types,
                interpretation=diversification_interpretation,
                risk_level=diversification_risk,
                methodology="Count of different investment types with material balances"
            ))

        # Investment Performance vs Core Business
        total_investment_income = (
                income_statement.get('investment_income', 0) +
                income_statement.get('share_of_associate_profits', 0) +
                income_statement.get('share_of_jv_profits', 0)
        )

        operating_income = income_statement.get('operating_income', 0)

        if operating_income > 0 and total_investment_income != 0:
            investment_contribution = self.safe_divide(total_investment_income, operating_income)

            contribution_interpretation = "Investments significantly contribute to earnings" if investment_contribution > 0.3 else "Moderate investment contribution" if investment_contribution > 0.1 else "Limited investment contribution to earnings"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Investment Earnings Contribution",
                value=investment_contribution,
                interpretation=contribution_interpretation,
                risk_level=RiskLevel.LOW,
                methodology="Total Investment Income / Operating Income"
            ))

        return results

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key intercorporate investment metrics"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        metrics = {}

        # Investment intensities
        total_assets = balance_sheet.get('total_assets', 0)

        if total_assets > 0:
            financial_assets = (
                    balance_sheet.get('trading_securities', 0) +
                    balance_sheet.get('available_for_sale_securities', 0) +
                    balance_sheet.get('held_to_maturity_securities', 0)
            )
            metrics['financial_asset_intensity'] = self.safe_divide(financial_assets, total_assets)

            associates = balance_sheet.get('investments_in_associates', 0)
            metrics['associate_intensity'] = self.safe_divide(associates, total_assets)

            joint_ventures = balance_sheet.get('investments_in_joint_ventures', 0)
            metrics['joint_venture_intensity'] = self.safe_divide(joint_ventures, total_assets)

            total_investments = financial_assets + associates + joint_ventures
            metrics['total_investment_intensity'] = self.safe_divide(total_investments, total_assets)

        # Investment returns
        associate_investments = balance_sheet.get('investments_in_associates', 0)
        if associate_investments > 0:
            share_of_profits = income_statement.get('share_of_associate_profits', 0)
            metrics['equity_method_return'] = self.safe_divide(share_of_profits, associate_investments)

        jv_investments = balance_sheet.get('investments_in_joint_ventures', 0)
        if jv_investments > 0:
            jv_profits = income_statement.get('share_of_jv_profits', 0)
            metrics['joint_venture_return'] = self.safe_divide(jv_profits, jv_investments)

        # Goodwill metrics
        goodwill = balance_sheet.get('goodwill', 0)
        if total_assets > 0:
            metrics['goodwill_intensity'] = self.safe_divide(goodwill, total_assets)

        return metrics

    def create_financial_asset_analysis(self, statements: FinancialStatements) -> FinancialAssetAnalysis:
        """Create comprehensive financial asset analysis object"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        # Extract financial asset components
        classification_breakdown = {
            'trading_securities': balance_sheet.get('trading_securities', 0),
            'available_for_sale': balance_sheet.get('available_for_sale_securities', 0),
            'held_to_maturity': balance_sheet.get('held_to_maturity_securities', 0),
            'fvtpl_assets': balance_sheet.get('fvtpl_financial_assets', 0),
            'fvoci_assets': balance_sheet.get('fvoci_financial_assets', 0),
            'amortized_cost': balance_sheet.get('amortized_cost_financial_assets', 0)
        }

        total_financial_assets = sum(classification_breakdown.values())

        # Calculate fair value vs amortized cost
        fair_value_assets = (
                classification_breakdown['trading_securities'] +
                classification_breakdown['available_for_sale'] +
                classification_breakdown['fvtpl_assets'] +
                classification_breakdown['fvoci_assets']
        )

        amortized_cost_assets = (
                classification_breakdown['held_to_maturity'] +
                classification_breakdown['amortized_cost']
        )

        # Investment performance metrics
        investment_income = income_statement.get('investment_income', 0)
        realized_gains = income_statement.get('realized_investment_gains', 0)
        unrealized_gains = income_statement.get('unrealized_investment_gains', 0)

        investment_returns = self.safe_divide(investment_income + realized_gains,
                                              total_financial_assets) if total_financial_assets > 0 else 0
        dividend_income = income_statement.get('dividend_income', 0)

        # Risk assessment (simplified)
        market_risk_exposure = self.safe_divide(fair_value_assets,
                                                total_financial_assets) if total_financial_assets > 0 else 0

        liquidity_risk = RiskLevel.LOW if amortized_cost_assets > fair_value_assets else RiskLevel.MODERATE if market_risk_exposure < 0.7 else RiskLevel.HIGH

        return FinancialAssetAnalysis(
            total_financial_assets=total_financial_assets,
            classification_breakdown=classification_breakdown,
            fair_value_assets=fair_value_assets,
            amortized_cost_assets=amortized_cost_assets,
            unrealized_gains_losses=unrealized_gains,
            market_risk_exposure=market_risk_exposure,
            liquidity_risk=liquidity_risk,
            investment_returns=investment_returns,
            dividend_income=dividend_income
        )

    def create_associate_analysis(self, statements: FinancialStatements,
                                  comparative_data: Optional[List[FinancialStatements]] = None) -> AssociateAnalysis:
        """Create comprehensive associate analysis object"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        total_associate_investments = balance_sheet.get('investments_in_associates', 0)
        number_of_associates = notes.get('number_of_associates', 1)  # Default to 1 if not disclosed

        share_of_profits_losses = income_statement.get('share_of_associate_profits', 0)
        dividend_income_associates = income_statement.get('dividends_from_associates', 0)

        # Calculate carrying value change
        carrying_value_change = 0
        if comparative_data and len(comparative_data) > 0:
            prev_investments = comparative_data[-1].balance_sheet.get('investments_in_associates', 0)
            carrying_value_change = total_associate_investments - prev_investments

        # Performance metrics
        associate_roe = self.safe_divide(share_of_profits_losses,
                                         total_associate_investments) if total_associate_investments > 0 else 0

        # Trend analysis
        associate_performance_trend = TrendDirection.STABLE
        if comparative_data and len(comparative_data) >= 2:
            profit_values = []
            for past_statements in comparative_data:
                past_profits = past_statements.income_statement.get('share_of_associate_profits', 0)
                profit_values.append(past_profits)
            profit_values.append(share_of_profits_losses)

            if len(profit_values) >= 3:
                if profit_values[-1] > profit_values[0] * 1.1:
                    associate_performance_trend = TrendDirection.IMPROVING
                elif profit_values[-1] < profit_values[0] * 0.9:
                    associate_performance_trend = TrendDirection.DETERIORATING

        # Impairment indicators
        impairment_indicators = []
        if share_of_profits_losses < 0:
            impairment_indicators.append("Associates generating losses")
        if associate_roe < 0.05 and associate_roe > 0:
            impairment_indicators.append("Low return on associate investments")

        return AssociateAnalysis(
            total_associate_investments=total_associate_investments,
            number_of_associates=number_of_associates,
            share_of_profits_losses=share_of_profits_losses,
            dividend_income_associates=dividend_income_associates,
            carrying_value_change=carrying_value_change,
            associate_roe=associate_roe,
            associate_performance_trend=associate_performance_trend,
            impairment_indicators=impairment_indicators
        )