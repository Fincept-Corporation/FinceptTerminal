
"""
Financial Statement Inventory Analysis Module
========================================

Inventory analysis and working capital assessment

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


class InventoryMethod(Enum):
    """Inventory valuation methods"""
    FIFO = "fifo"
    LIFO = "lifo"
    WEIGHTED_AVERAGE = "weighted_average"
    SPECIFIC_IDENTIFICATION = "specific_identification"


class InventoryType(Enum):
    """Types of inventory"""
    RAW_MATERIALS = "raw_materials"
    WORK_IN_PROCESS = "work_in_process"
    FINISHED_GOODS = "finished_goods"
    MERCHANDISE = "merchandise"
    TOTAL = "total"


class EconomicEnvironment(Enum):
    """Economic environment classification"""
    INFLATIONARY = "inflationary"
    DEFLATIONARY = "deflationary"
    STABLE = "stable"


@dataclass
class InventoryValuationAnalysis:
    """Comprehensive inventory valuation analysis"""
    cost_method: InventoryMethod
    current_inventory_value: float
    inventory_reserve: float
    net_realizable_value: float
    lower_of_cost_nrv: float

    # Valuation impact analysis
    fifo_equivalent_value: float = None
    lifo_equivalent_value: float = None
    lifo_reserve: float = None

    # Quality indicators
    inventory_quality_score: float = 0.0
    obsolescence_indicators: List[str] = field(default_factory=list)
    valuation_concerns: List[str] = field(default_factory=list)


@dataclass
class InventoryEfficiencyAnalysis:
    """Inventory efficiency and turnover analysis"""
    inventory_turnover: float
    days_inventory_outstanding: float
    inventory_to_sales_ratio: float
    inventory_growth_rate: float

    # Trend analysis
    turnover_trend: TrendDirection
    efficiency_score: float

    # Comparative metrics
    industry_comparison: str = None
    seasonal_adjustments: float = None


@dataclass
class InflationImpactAnalysis:
    """Analysis of inflation/deflation effects on inventory"""
    economic_environment: EconomicEnvironment
    inflation_rate: float

    # FIFO vs LIFO impacts
    fifo_impact_on_cogs: float
    fifo_impact_on_gross_margin: float
    fifo_impact_on_inventory_value: float

    lifo_impact_on_cogs: float
    lifo_impact_on_gross_margin: float
    lifo_impact_on_inventory_value: float

    # Tax implications
    tax_advantage_method: str
    estimated_tax_benefit: float = None


class InventoryAnalyzer(BaseAnalyzer):
    """
    Comprehensive inventory analyzer implementing CFA Institute standards.
    Covers valuation methods, efficiency analysis, and inflation impact assessment.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_inventory_formulas()
        self._initialize_inventory_benchmarks()

    def _initialize_inventory_formulas(self):
        """Initialize inventory-specific formulas"""
        self.formula_registry.update({
            'inventory_turnover': lambda cogs, avg_inventory: self.safe_divide(cogs, avg_inventory),
            'days_inventory_outstanding': lambda avg_inventory, daily_cogs: self.safe_divide(avg_inventory, daily_cogs),
            'inventory_to_sales': lambda inventory, revenue: self.safe_divide(inventory, revenue),
            'gross_margin_fifo': lambda revenue, cogs_fifo: self.safe_divide(revenue - cogs_fifo, revenue),
            'gross_margin_lifo': lambda revenue, cogs_lifo: self.safe_divide(revenue - cogs_lifo, revenue),
            'lifo_reserve_ratio': lambda lifo_reserve, total_inventory: self.safe_divide(lifo_reserve, total_inventory)
        })

    def _initialize_inventory_benchmarks(self):
        """Initialize inventory-specific benchmarks"""
        # Industry-dependent benchmarks (these are general guidelines)
        self.inventory_benchmarks = {
            'inventory_turnover': {
                'retail': {'excellent': 12.0, 'good': 8.0, 'adequate': 6.0, 'poor': 4.0},
                'manufacturing': {'excellent': 8.0, 'good': 6.0, 'adequate': 4.0, 'poor': 2.0},
                'general': {'excellent': 10.0, 'good': 7.0, 'adequate': 5.0, 'poor': 3.0}
            },
            'days_inventory_outstanding': {
                'retail': {'excellent': 30, 'good': 45, 'adequate': 60, 'poor': 90},
                'manufacturing': {'excellent': 45, 'good': 60, 'adequate': 90, 'poor': 120},
                'general': {'excellent': 36, 'good': 52, 'adequate': 73, 'poor': 120}
            },
            'inventory_to_sales': {
                'retail': {'excellent': 0.08, 'good': 0.12, 'adequate': 0.15, 'poor': 0.20},
                'manufacturing': {'excellent': 0.15, 'good': 0.20, 'adequate': 0.25, 'poor': 0.35},
                'general': {'excellent': 0.10, 'good': 0.15, 'adequate': 0.20, 'poor': 0.30}
            }
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive inventory analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all inventory aspects
        """
        results = []

        # Check if inventory analysis is applicable
        inventory = statements.balance_sheet.get('inventory', 0)
        if inventory <= 0:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Inventory Analysis",
                value=0.0,
                interpretation="No inventory reported - inventory analysis not applicable",
                risk_level=RiskLevel.LOW,
                methodology="Balance sheet inventory examination"
            ))
            return results

        # Core inventory efficiency analysis
        results.extend(self._analyze_inventory_efficiency(statements, comparative_data, industry_data))

        # Inventory valuation analysis
        results.extend(self._analyze_inventory_valuation(statements, comparative_data))

        # Lower of cost and NRV analysis
        results.extend(self._analyze_lower_cost_nrv(statements, comparative_data))

        # Inflation/deflation impact analysis
        results.extend(self._analyze_inflation_impact(statements, comparative_data, industry_data))

        # Inventory composition analysis
        results.extend(self._analyze_inventory_composition(statements))

        # Disclosure quality assessment
        results.extend(self._assess_inventory_disclosures(statements))

        return results

    def _analyze_inventory_efficiency(self, statements: FinancialStatements,
                                      comparative_data: Optional[List[FinancialStatements]] = None,
                                      industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze inventory efficiency and turnover metrics"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        inventory = balance_sheet.get('inventory', 0)
        cost_of_sales = income_statement.get('cost_of_sales', 0)
        revenue = income_statement.get('revenue', 0)

        # Calculate average inventory if historical data available
        avg_inventory = inventory
        if comparative_data and len(comparative_data) > 0:
            prev_inventory = comparative_data[-1].balance_sheet.get('inventory', 0)
            if prev_inventory > 0:
                avg_inventory = (inventory + prev_inventory) / 2

        # Inventory Turnover Ratio
        if avg_inventory > 0 and cost_of_sales > 0:
            inventory_turnover = self.safe_divide(cost_of_sales, avg_inventory)

            # Get appropriate benchmark
            industry_type = industry_data.get('type', 'general') if industry_data else 'general'
            benchmark = self.inventory_benchmarks['inventory_turnover'].get(industry_type,
                                                                            self.inventory_benchmarks[
                                                                                'inventory_turnover']['general'])

            risk_level = self.assess_risk_level(inventory_turnover, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Inventory Turnover",
                value=inventory_turnover,
                interpretation=self.generate_interpretation("inventory turnover", inventory_turnover, risk_level,
                                                            AnalysisType.ACTIVITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(inventory_turnover, industry_data.get(
                    'inventory_turnover') if industry_data else None),
                methodology="Cost of Goods Sold / Average Inventory",
                limitations=["Seasonality may affect single-period calculations"]
            ))

        # Days Inventory Outstanding (DIO)
        if avg_inventory > 0 and cost_of_sales > 0:
            daily_cogs = cost_of_sales / 365
            days_inventory = self.safe_divide(avg_inventory, daily_cogs)

            # Get appropriate benchmark
            benchmark = self.inventory_benchmarks['days_inventory_outstanding'].get(industry_type,
                                                                                    self.inventory_benchmarks[
                                                                                        'days_inventory_outstanding'][
                                                                                        'general'])

            risk_level = self.assess_risk_level(days_inventory, benchmark, higher_is_better=False)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Days Inventory Outstanding",
                value=days_inventory,
                interpretation=f"Inventory held for {days_inventory:.0f} days on average",
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(days_inventory, industry_data.get(
                    'days_inventory') if industry_data else None),
                methodology="(Average Inventory / COGS) × 365",
                limitations=["Does not account for seasonal inventory patterns"]
            ))

        # Inventory to Sales Ratio
        if revenue > 0:
            inventory_to_sales = self.safe_divide(inventory, revenue)

            benchmark = self.inventory_benchmarks['inventory_to_sales'].get(industry_type,
                                                                            self.inventory_benchmarks[
                                                                                'inventory_to_sales']['general'])

            risk_level = self.assess_risk_level(inventory_to_sales, benchmark, higher_is_better=False)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Inventory to Sales Ratio",
                value=inventory_to_sales,
                interpretation=f"Inventory represents {self.format_percentage(inventory_to_sales)} of annual sales",
                risk_level=risk_level,
                methodology="Ending Inventory / Revenue",
                limitations=["Point-in-time measure may not reflect average levels"]
            ))

        # Inventory growth analysis
        if comparative_data and len(comparative_data) > 0:
            prev_inventory = comparative_data[-1].balance_sheet.get('inventory', 0)
            prev_revenue = comparative_data[-1].income_statement.get('revenue', 0)

            if prev_inventory > 0:
                inventory_growth = (inventory / prev_inventory) - 1

                if prev_revenue > 0:
                    revenue_growth = (revenue / prev_revenue) - 1

                    # Compare inventory growth to sales growth
                    if abs(revenue_growth) > 0.01:  # Avoid division by very small numbers
                        growth_comparison = inventory_growth - revenue_growth

                        if growth_comparison > 0.1:
                            growth_interpretation = "Inventory growing faster than sales - potential build-up"
                            growth_risk = RiskLevel.MODERATE
                        elif growth_comparison < -0.1:
                            growth_interpretation = "Inventory growing slower than sales - improving efficiency"
                            growth_risk = RiskLevel.LOW
                        else:
                            growth_interpretation = "Inventory growth aligned with sales growth"
                            growth_risk = RiskLevel.LOW

                        results.append(AnalysisResult(
                            analysis_type=AnalysisType.ACTIVITY,
                            metric_name="Inventory vs Sales Growth",
                            value=growth_comparison,
                            interpretation=growth_interpretation,
                            risk_level=growth_risk,
                            methodology="Inventory Growth Rate - Revenue Growth Rate",
                            limitations=["Single period comparison - trend analysis preferred"]
                        ))

        return results

    def _analyze_inventory_valuation(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze inventory valuation methods and their impact"""
        results = []

        notes = statements.notes
        balance_sheet = statements.balance_sheet

        inventory = balance_sheet.get('inventory', 0)

        # Check for LIFO reserve disclosure
        lifo_reserve = notes.get('lifo_reserve', 0)

        if lifo_reserve > 0:
            # LIFO Reserve Analysis
            lifo_reserve_ratio = self.safe_divide(lifo_reserve, inventory)

            reserve_interpretation = "Significant LIFO reserve indicates substantial inflation impact" if lifo_reserve_ratio > 0.2 else "Moderate LIFO reserve" if lifo_reserve_ratio > 0.1 else "Small LIFO reserve impact"
            reserve_risk = RiskLevel.MODERATE if lifo_reserve_ratio > 0.3 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="LIFO Reserve Ratio",
                value=lifo_reserve_ratio,
                interpretation=reserve_interpretation,
                risk_level=reserve_risk,
                methodology="LIFO Reserve / Total Inventory",
                limitations=["LIFO reserve represents cumulative impact over multiple periods"]
            ))

            # FIFO-equivalent inventory value
            fifo_equivalent_inventory = inventory + lifo_reserve
            fifo_adjustment_ratio = self.safe_divide(lifo_reserve, inventory)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="FIFO Equivalent Adjustment",
                value=fifo_adjustment_ratio,
                interpretation=f"FIFO inventory would be {self.format_percentage(fifo_adjustment_ratio)} higher than LIFO",
                risk_level=RiskLevel.LOW,
                methodology="LIFO Reserve / LIFO Inventory",
                limitations=["Adjustment provides approximate FIFO equivalent"]
            ))

        # Inventory method impact analysis
        cost_method = notes.get('inventory_method', 'unknown')
        if cost_method != 'unknown':
            method_risk_assessment = self._assess_method_appropriateness(cost_method, statements)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Inventory Method Assessment",
                value=1.0,
                interpretation=f"Company uses {cost_method} method - {method_risk_assessment['assessment']}",
                risk_level=method_risk_assessment['risk'],
                methodology="Qualitative assessment of inventory method appropriateness",
                limitations=method_risk_assessment['limitations']
            ))

        return results

    def _analyze_lower_cost_nrv(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze lower of cost and net realizable value measurements"""
        results = []

        balance_sheet = statements.balance_sheet
        notes = statements.notes

        inventory = balance_sheet.get('inventory', 0)
        inventory_writedown = notes.get('inventory_writedown', 0)
        inventory_reserve = notes.get('inventory_obsolescence_reserve', 0)

        # Inventory writedown analysis
        if inventory_writedown > 0:
            writedown_ratio = self.safe_divide(inventory_writedown, inventory + inventory_writedown)

            writedown_interpretation = "Significant inventory writedown indicates valuation issues" if writedown_ratio > 0.05 else "Moderate inventory adjustment" if writedown_ratio > 0.02 else "Minor inventory writedown"
            writedown_risk = RiskLevel.HIGH if writedown_ratio > 0.1 else RiskLevel.MODERATE if writedown_ratio > 0.05 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Inventory Writedown Impact",
                value=writedown_ratio,
                interpretation=writedown_interpretation,
                risk_level=writedown_risk,
                methodology="Inventory Writedown / (Inventory + Writedown)",
                limitations=["Writedowns may indicate obsolescence or market decline"]
            ))

        # Obsolescence reserve analysis
        if inventory_reserve > 0:
            reserve_ratio = self.safe_divide(inventory_reserve, inventory + inventory_reserve)

            reserve_interpretation = "High obsolescence reserve suggests inventory quality concerns" if reserve_ratio > 0.1 else "Moderate obsolescence provision" if reserve_ratio > 0.05 else "Conservative obsolescence reserve"
            reserve_risk = RiskLevel.MODERATE if reserve_ratio > 0.15 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Obsolescence Reserve Ratio",
                value=reserve_ratio,
                interpretation=reserve_interpretation,
                risk_level=reserve_risk,
                methodology="Obsolescence Reserve / (Inventory + Reserve)",
                limitations=["Reserve adequacy depends on inventory composition and age"]
            ))

        # NRV compliance assessment
        results.extend(self._assess_nrv_compliance(statements, comparative_data))

        return results

    def _analyze_inflation_impact(self, statements: FinancialStatements,
                                  comparative_data: Optional[List[FinancialStatements]] = None,
                                  industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze impact of inflation/deflation on inventory and ratios"""
        results = []

        notes = statements.notes
        income_statement = statements.income_statement

        # Determine economic environment
        inflation_rate = industry_data.get('inflation_rate', 0) if industry_data else 0
        economic_environment = self._determine_economic_environment(inflation_rate)

        cost_method = notes.get('inventory_method', 'unknown')
        lifo_reserve = notes.get('lifo_reserve', 0)

        if economic_environment != EconomicEnvironment.STABLE and cost_method in ['fifo', 'lifo']:
            # Inflation impact on COGS and margins
            revenue = income_statement.get('revenue', 0)
            cost_of_sales = income_statement.get('cost_of_sales', 0)

            if revenue > 0 and cost_of_sales > 0:
                current_gross_margin = self.safe_divide(revenue - cost_of_sales, revenue)

                # Estimate impact of different methods
                if cost_method == 'lifo' and lifo_reserve > 0:
                    # Estimate FIFO COGS
                    estimated_fifo_cogs = cost_of_sales - lifo_reserve  # Simplified estimation
                    estimated_fifo_margin = self.safe_divide(revenue - estimated_fifo_cogs, revenue)

                    margin_impact = estimated_fifo_margin - current_gross_margin

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name="Inflation Method Impact",
                        value=margin_impact,
                        interpretation=f"FIFO would result in {self.format_percentage(abs(margin_impact))} {'higher' if margin_impact > 0 else 'lower'} gross margin",
                        risk_level=RiskLevel.MODERATE if abs(margin_impact) > 0.05 else RiskLevel.LOW,
                        methodology="Estimated FIFO margin - Current LIFO margin",
                        limitations=["Estimation based on LIFO reserve approximation"]
                    ))

                # Tax implications
                if economic_environment == EconomicEnvironment.INFLATIONARY:
                    tax_preferred_method = "LIFO" if cost_method == 'lifo' else "LIFO (not used)"
                    tax_impact_description = "LIFO provides tax benefits in inflationary environment" if cost_method == 'lifo' else "FIFO results in higher taxable income during inflation"

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name="Tax Method Efficiency",
                        value=1.0 if cost_method == 'lifo' else 0.0,
                        interpretation=tax_impact_description,
                        risk_level=RiskLevel.LOW if cost_method == 'lifo' else RiskLevel.MODERATE,
                        methodology="Qualitative assessment of method choice in inflationary environment"
                    ))

        return results

    def _analyze_inventory_composition(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Analyze inventory composition and mix"""
        results = []

        notes = statements.notes
        balance_sheet = statements.balance_sheet

        total_inventory = balance_sheet.get('inventory', 0)

        # Analyze inventory components if disclosed
        inventory_components = {
            'raw_materials': notes.get('raw_materials_inventory', 0),
            'work_in_process': notes.get('wip_inventory', 0),
            'finished_goods': notes.get('finished_goods_inventory', 0)
        }

        total_components = sum(inventory_components.values())

        if total_components > 0 and abs(total_components - total_inventory) / total_inventory < 0.1:
            # Composition analysis
            for component, value in inventory_components.items():
                if value > 0:
                    component_ratio = self.safe_divide(value, total_inventory)

                    component_name = component.replace('_', ' ').title()

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.ACTIVITY,
                        metric_name=f"{component_name} Composition",
                        value=component_ratio,
                        interpretation=f"{component_name} represents {self.format_percentage(component_ratio)} of total inventory",
                        risk_level=RiskLevel.LOW,
                        methodology=f"{component_name} / Total Inventory"
                    ))

            # Risk assessment based on composition
            raw_materials_ratio = inventory_components['raw_materials'] / total_inventory
            wip_ratio = inventory_components['work_in_process'] / total_inventory
            finished_goods_ratio = inventory_components['finished_goods'] / total_inventory

            if wip_ratio > 0.5:
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Inventory Composition Risk",
                    value=wip_ratio,
                    interpretation="High work-in-process ratio may indicate production inefficiencies",
                    risk_level=RiskLevel.MODERATE,
                    methodology="Qualitative assessment of inventory composition"
                ))
            elif finished_goods_ratio > 0.7:
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Inventory Composition Risk",
                    value=finished_goods_ratio,
                    interpretation="High finished goods ratio may indicate demand forecasting issues",
                    risk_level=RiskLevel.MODERATE,
                    methodology="Qualitative assessment of inventory composition"
                ))

        return results

    def _assess_inventory_disclosures(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Assess quality and completeness of inventory disclosures"""
        results = []

        notes = statements.notes

        # Required disclosure checklist
        required_disclosures = {
            'inventory_method': 'Accounting policy for inventory valuation',
            'inventory_composition': 'Breakdown of inventory components',
            'writedown_policy': 'Policy for inventory writedowns',
            'obsolescence_assessment': 'Obsolescence evaluation methodology'
        }

        disclosure_score = 0
        missing_disclosures = []

        for disclosure_key, description in required_disclosures.items():
            if any(disclosure_key in key.lower() for key in notes.keys()):
                disclosure_score += 25
            else:
                missing_disclosures.append(description)

        disclosure_interpretation = "Comprehensive inventory disclosures" if disclosure_score > 75 else "Adequate inventory disclosures" if disclosure_score > 50 else "Limited inventory disclosures"
        disclosure_risk = RiskLevel.LOW if disclosure_score > 75 else RiskLevel.MODERATE if disclosure_score > 50 else RiskLevel.HIGH

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Inventory Disclosure Quality",
            value=disclosure_score,
            interpretation=disclosure_interpretation,
            risk_level=disclosure_risk,
            methodology="Assessment of required inventory disclosure completeness",
            limitations=missing_disclosures if missing_disclosures else [
                "Disclosure quality assessment based on available notes"]
        ))

        return results

    def _assess_method_appropriateness(self, method: str, statements: FinancialStatements) -> Dict[
        str, Union[str, RiskLevel, List[str]]]:
        """Assess appropriateness of inventory method choice"""

        notes = statements.notes

        # Industry and business considerations
        business_type = notes.get('business_description', '').lower()

        if method.lower() == 'fifo':
            if 'perishable' in business_type or 'food' in business_type:
                return {
                    'assessment': 'FIFO appropriate for perishable goods business',
                    'risk': RiskLevel.LOW,
                    'limitations': ['FIFO reflects physical flow for perishables']
                }
            else:
                return {
                    'assessment': 'FIFO provides current cost basis for inventory',
                    'risk': RiskLevel.LOW,
                    'limitations': ['FIFO may overstate profits during inflation']
                }

        elif method.lower() == 'lifo':
            return {
                'assessment': 'LIFO provides tax benefits in inflationary periods',
                'risk': RiskLevel.LOW,
                'limitations': ['LIFO may understate inventory values', 'Not permitted under IFRS']
            }

        elif method.lower() == 'weighted_average':
            return {
                'assessment': 'Weighted average smooths cost fluctuations',
                'risk': RiskLevel.LOW,
                'limitations': ['May not reflect specific cost identification']
            }

        else:
            return {
                'assessment': 'Method appropriateness cannot be assessed',
                'risk': RiskLevel.MODERATE,
                'limitations': ['Insufficient information on inventory method']
            }

    def _assess_nrv_compliance(self, statements: FinancialStatements,
                               comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Assess compliance with lower of cost and NRV requirements"""
        results = []

        notes = statements.notes
        income_statement = statements.income_statement

        # Look for NRV-related disclosures
        nrv_writedowns = notes.get('nrv_writedowns', 0)
        inventory_impairment = income_statement.get('inventory_impairment', 0)

        if nrv_writedowns > 0 or inventory_impairment > 0:
            total_writedowns = nrv_writedowns + inventory_impairment

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="NRV Writedown Activity",
                value=total_writedowns,
                interpretation=f"NRV writedowns of ${total_writedowns:,.0f} indicate active impairment monitoring",
                risk_level=RiskLevel.MODERATE if total_writedowns > 0 else RiskLevel.LOW,
                methodology="Sum of NRV writedowns and inventory impairments",
                limitations=["Writedowns may indicate market deterioration or obsolescence"]
            ))

        # Assess NRV methodology disclosure
        nrv_policy = notes.get('nrv_methodology', '')
        if nrv_policy:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="NRV Methodology Disclosure",
                value=1.0,
                interpretation="Company discloses NRV assessment methodology",
                risk_level=RiskLevel.LOW,
                methodology="Qualitative assessment of NRV disclosure quality"
            ))
        else:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="NRV Methodology Disclosure",
                value=0.0,
                interpretation="Limited disclosure of NRV assessment methodology",
                risk_level=RiskLevel.MODERATE,
                methodology="Qualitative assessment of NRV disclosure quality",
                limitations=["Lack of NRV methodology disclosure reduces transparency"]
            ))

        return results

    def _determine_economic_environment(self, inflation_rate: float) -> EconomicEnvironment:
        """Determine economic environment based on inflation rate"""
        if inflation_rate > 0.03:  # 3% threshold
            return EconomicEnvironment.INFLATIONARY
        elif inflation_rate < -0.01:  # -1% threshold
            return EconomicEnvironment.DEFLATIONARY
        else:
            return EconomicEnvironment.STABLE

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key inventory metrics"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        inventory = balance_sheet.get('inventory', 0)
        cost_of_sales = income_statement.get('cost_of_sales', 0)
        revenue = income_statement.get('revenue', 0)

        metrics = {}

        # Core inventory metrics
        if inventory > 0:
            metrics['inventory_value'] = inventory

            if cost_of_sales > 0:
                metrics['inventory_turnover'] = self.safe_divide(cost_of_sales, inventory)
                metrics['days_inventory_outstanding'] = self.safe_divide(inventory * 365, cost_of_sales)

            if revenue > 0:
                metrics['inventory_to_sales'] = self.safe_divide(inventory, revenue)

        # LIFO reserve metrics
        notes = statements.notes
        lifo_reserve = notes.get('lifo_reserve', 0)
        if lifo_reserve > 0:
            metrics['lifo_reserve'] = lifo_reserve
            metrics['lifo_reserve_ratio'] = self.safe_divide(lifo_reserve, inventory)

        return metrics

    def create_inventory_efficiency_analysis(self, statements: FinancialStatements,
                                             comparative_data: Optional[
                                                 List[FinancialStatements]] = None) -> InventoryEfficiencyAnalysis:
        """Create comprehensive inventory efficiency analysis object"""

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        inventory = balance_sheet.get('inventory', 0)
        cost_of_sales = income_statement.get('cost_of_sales', 0)
        revenue = income_statement.get('revenue', 0)

        # Calculate average inventory
        avg_inventory = inventory
        if comparative_data and len(comparative_data) > 0:
            prev_inventory = comparative_data[-1].balance_sheet.get('inventory', 0)
            if prev_inventory > 0:
                avg_inventory = (inventory + prev_inventory) / 2

        # Core efficiency metrics
        inventory_turnover = self.safe_divide(cost_of_sales, avg_inventory)
        days_inventory_outstanding = self.safe_divide(avg_inventory * 365, cost_of_sales) if cost_of_sales > 0 else 0
        inventory_to_sales_ratio = self.safe_divide(inventory, revenue)

        # Growth rate calculation
        inventory_growth_rate = 0
        if comparative_data and len(comparative_data) > 0:
            prev_inventory = comparative_data[-1].balance_sheet.get('inventory', 0)
            if prev_inventory > 0:
                inventory_growth_rate = (inventory / prev_inventory) - 1

        # Trend analysis
        turnover_trend = TrendDirection.STABLE
        if comparative_data and len(comparative_data) >= 2:
            turnover_values = []
            for past_statements in comparative_data:
                past_inventory = past_statements.balance_sheet.get('inventory', 0)
                past_cogs = past_statements.income_statement.get('cost_of_sales', 0)
                if past_inventory > 0 and past_cogs > 0:
                    turnover_values.append(past_cogs / past_inventory)

            if len(turnover_values) >= 2:
                if turnover_values[-1] > turnover_values[0] * 1.05:
                    turnover_trend = TrendDirection.IMPROVING
                elif turnover_values[-1] < turnover_values[0] * 0.95:
                    turnover_trend = TrendDirection.DETERIORATING

        # Efficiency score calculation
        efficiency_score = 100
        if inventory_turnover < 4:
            efficiency_score -= 20
        if days_inventory_outstanding > 90:
            efficiency_score -= 15
        if inventory_to_sales_ratio > 0.25:
            efficiency_score -= 10

        efficiency_score = max(0, efficiency_score)

        return InventoryEfficiencyAnalysis(
            inventory_turnover=inventory_turnover,
            days_inventory_outstanding=days_inventory_outstanding,
            inventory_to_sales_ratio=inventory_to_sales_ratio,
            inventory_growth_rate=inventory_growth_rate,
            turnover_trend=turnover_trend,
            efficiency_score=efficiency_score
        )

    def create_inventory_valuation_analysis(self, statements: FinancialStatements) -> InventoryValuationAnalysis:
        """Create comprehensive inventory valuation analysis object"""

        balance_sheet = statements.balance_sheet
        notes = statements.notes

        inventory = balance_sheet.get('inventory', 0)
        cost_method_str = notes.get('inventory_method', 'unknown')

        # Determine cost method
        cost_method = InventoryMethod.FIFO  # default
        if 'lifo' in cost_method_str.lower():
            cost_method = InventoryMethod.LIFO
        elif 'weighted' in cost_method_str.lower() or 'average' in cost_method_str.lower():
            cost_method = InventoryMethod.WEIGHTED_AVERAGE
        elif 'specific' in cost_method_str.lower():
            cost_method = InventoryMethod.SPECIFIC_IDENTIFICATION

        # Valuation components
        inventory_reserve = notes.get('inventory_obsolescence_reserve', 0)
        lifo_reserve = notes.get('lifo_reserve', 0)

        # Estimate NRV and lower of cost/NRV
        net_realizable_value = inventory  # Would need market data for actual calculation
        lower_of_cost_nrv = inventory - inventory_reserve

        # FIFO/LIFO equivalent calculations
        fifo_equivalent_value = None
        lifo_equivalent_value = None

        if cost_method == InventoryMethod.LIFO and lifo_reserve > 0:
            fifo_equivalent_value = inventory + lifo_reserve
        elif cost_method == InventoryMethod.FIFO and lifo_reserve > 0:
            lifo_equivalent_value = inventory - lifo_reserve

        # Quality assessment
        quality_score = 100
        obsolescence_indicators = []
        valuation_concerns = []

        if inventory_reserve > 0:
            reserve_ratio = inventory_reserve / (inventory + inventory_reserve)
            if reserve_ratio > 0.1:
                quality_score -= 20
                obsolescence_indicators.append("High obsolescence reserve")

        if cost_method == InventoryMethod.LIFO:
            valuation_concerns.append("LIFO may understate current inventory values")

        return InventoryValuationAnalysis(
            cost_method=cost_method,
            current_inventory_value=inventory,
            inventory_reserve=inventory_reserve,
            net_realizable_value=net_realizable_value,
            lower_of_cost_nrv=lower_of_cost_nrv,
            fifo_equivalent_value=fifo_equivalent_value,
            lifo_equivalent_value=lifo_equivalent_value,
            lifo_reserve=lifo_reserve,
            inventory_quality_score=quality_score,
            obsolescence_indicators=obsolescence_indicators,
            valuation_concerns=valuation_concerns
        )

    def create_inflation_impact_analysis(self, statements: FinancialStatements,
                                         inflation_rate: float = 0.0) -> InflationImpactAnalysis:
        """Create inflation impact analysis object"""

        income_statement = statements.income_statement
        notes = statements.notes

        economic_environment = self._determine_economic_environment(inflation_rate)
        cost_method = notes.get('inventory_method', 'unknown')
        lifo_reserve = notes.get('lifo_reserve', 0)

        revenue = income_statement.get('revenue', 0)
        cost_of_sales = income_statement.get('cost_of_sales', 0)

        # Initialize impact metrics
        fifo_impact_on_cogs = 0
        fifo_impact_on_gross_margin = 0
        fifo_impact_on_inventory_value = 0

        lifo_impact_on_cogs = 0
        lifo_impact_on_gross_margin = 0
        lifo_impact_on_inventory_value = 0

        tax_advantage_method = "No significant difference"

        # Calculate impacts if using LIFO and LIFO reserve available
        if cost_method.lower() == 'lifo' and lifo_reserve > 0 and revenue > 0:
            current_gross_margin = (revenue - cost_of_sales) / revenue

            # Estimate FIFO COGS (simplified)
            estimated_fifo_cogs = cost_of_sales - lifo_reserve
            estimated_fifo_gross_margin = (revenue - estimated_fifo_cogs) / revenue

            fifo_impact_on_cogs = estimated_fifo_cogs - cost_of_sales
            fifo_impact_on_gross_margin = estimated_fifo_gross_margin - current_gross_margin
            fifo_impact_on_inventory_value = lifo_reserve

            if economic_environment == EconomicEnvironment.INFLATIONARY:
                tax_advantage_method = "LIFO provides tax advantage"

        elif cost_method.lower() == 'fifo' and inflation_rate > 0.02:
            # FIFO in inflationary environment
            if economic_environment == EconomicEnvironment.INFLATIONARY:
                tax_advantage_method = "LIFO would provide tax advantage (not used)"

        return InflationImpactAnalysis(
            economic_environment=economic_environment,
            inflation_rate=inflation_rate,
            fifo_impact_on_cogs=fifo_impact_on_cogs,
            fifo_impact_on_gross_margin=fifo_impact_on_gross_margin,
            fifo_impact_on_inventory_value=fifo_impact_on_inventory_value,
            lifo_impact_on_cogs=lifo_impact_on_cogs,
            lifo_impact_on_gross_margin=lifo_impact_on_gross_margin,
            lifo_impact_on_inventory_value=lifo_impact_on_inventory_value,
            tax_advantage_method=tax_advantage_method
        )

    def analyze_inventory_trends(self, current_statements: FinancialStatements,
                                 comparative_data: List[FinancialStatements]) -> Dict[str, ComparativeAnalysis]:
        """Analyze inventory trends over multiple periods"""

        trends = {}

        if not comparative_data:
            return trends

        # Collect inventory values over time
        inventory_values = []
        turnover_values = []
        periods = []

        for i, statements in enumerate(comparative_data):
            inventory = statements.balance_sheet.get('inventory', 0)
            cogs = statements.income_statement.get('cost_of_sales', 0)

            inventory_values.append(inventory)
            if inventory > 0 and cogs > 0:
                turnover_values.append(cogs / inventory)

            periods.append(f"Period-{len(comparative_data) - i}")

        # Add current period
        current_inventory = current_statements.balance_sheet.get('inventory', 0)
        current_cogs = current_statements.income_statement.get('cost_of_sales', 0)

        inventory_values.append(current_inventory)
        if current_inventory > 0 and current_cogs > 0:
            turnover_values.append(current_cogs / current_inventory)
        periods.append("Current")

        # Calculate trends
        if len(inventory_values) > 1:
            trends['inventory_values'] = self.calculate_trend(inventory_values, periods)

        if len(turnover_values) > 1:
            trends['inventory_turnover'] = self.calculate_trend(turnover_values, periods)

        return trends