
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
        """Comprehensive inventory analysis"""
        results = []
        inventory = statements.balance_sheet.get('inventory')
        if inventory is None or inventory <= 0:
            results.append(self.unavailable(
                AnalysisType.ACTIVITY, "Inventory Analysis",
                "inventory not reported" if inventory is None else "no inventory carried - analysis not applicable",
                "Balance sheet inventory examination"))
            return results

        results.extend(self._analyze_inventory_efficiency(statements, comparative_data, industry_data))
        results.extend(self._analyze_inventory_valuation(statements, comparative_data))
        results.extend(self._analyze_lower_cost_nrv(statements, comparative_data))
        results.extend(self._analyze_inflation_impact(statements, comparative_data, industry_data))
        results.extend(self._analyze_inventory_composition(statements))
        results.extend(self._assess_inventory_disclosures(statements))
        return results

    def _analyze_inventory_efficiency(self, statements: FinancialStatements,
                                      comparative_data: Optional[List[FinancialStatements]] = None,
                                      industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Inventory turnover, DIO, inventory/sales, inventory vs sales growth"""
        results = []
        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        inventory = balance_sheet['inventory']
        cost_of_sales = income_statement.get('cost_of_sales')
        revenue = income_statement.get('revenue')
        industry_type = str(industry_data.get('type', 'general')).lower() if isinstance(industry_data, dict) else 'general'

        def bench(name):
            table = self.inventory_benchmarks[name]
            return table.get(industry_type, table['general'])

        avg_inventory, basis = self.average_balance(statements, comparative_data, 'inventory')

        if cost_of_sales is not None and cost_of_sales > 0:
            inventory_turnover = cost_of_sales / avg_inventory
            risk_level = self.assess_risk_level(inventory_turnover, bench('inventory_turnover'), higher_is_better=True)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Inventory Turnover",
                value=inventory_turnover,
                interpretation=self.generate_interpretation("inventory turnover", inventory_turnover, risk_level,
                                                            AnalysisType.ACTIVITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(inventory_turnover, industry_data.get(
                    'inventory_turnover') if industry_data else None),
                methodology=f"Cost of Sales / Inventory ({self.basis_note(basis)})",
                limitations=["Seasonality may affect single-period calculations"]
            ))

            days_inventory = avg_inventory / cost_of_sales * 365
            risk_level = self.assess_risk_level(days_inventory, bench('days_inventory_outstanding'), higher_is_better=False)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Days Inventory Outstanding",
                value=days_inventory,
                interpretation=f"Inventory held for {days_inventory:.0f} days on average",
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(days_inventory, industry_data.get(
                    'days_inventory') if industry_data else None),
                methodology=f"Inventory ({self.basis_note(basis)}) / Cost of Sales x 365",
                limitations=["Does not account for seasonal inventory patterns"]
            ))
        else:
            results.append(self.unavailable(AnalysisType.ACTIVITY, "Inventory Turnover",
                                            "cost of sales not reported", "Cost of Sales / Average Inventory"))

        if revenue is not None and revenue > 0:
            inventory_to_sales = inventory / revenue
            risk_level = self.assess_risk_level(inventory_to_sales, bench('inventory_to_sales'), higher_is_better=False)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Inventory to Sales Ratio",
                value=inventory_to_sales,
                interpretation=f"Inventory represents {self.format_percentage(inventory_to_sales)} of annual sales",
                risk_level=risk_level,
                methodology="Ending Inventory / Revenue",
                limitations=["Point-in-time measure may not reflect average levels"]
            ))

        if comparative_data:
            inventory_growth = self.growth(inventory, self.prior_value(comparative_data, 'balance_sheet', 'inventory'))
            revenue_growth = self.growth(revenue, self.prior_value(comparative_data, 'income_statement', 'revenue'))
            if inventory_growth is not None and revenue_growth is not None:
                growth_comparison = inventory_growth - revenue_growth
                if growth_comparison > 0.1:
                    growth_interpretation, growth_risk = "Inventory growing faster than sales - potential build-up", RiskLevel.MODERATE
                elif growth_comparison < -0.1:
                    growth_interpretation, growth_risk = "Inventory growing slower than sales - improving efficiency", RiskLevel.LOW
                else:
                    growth_interpretation, growth_risk = "Inventory growth aligned with sales growth", RiskLevel.LOW
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
        """LIFO reserve and inventory method"""
        results = []
        notes = statements.notes
        inventory = statements.balance_sheet['inventory']

        lifo_reserve = self.note_number(notes, 'lifo_reserve')
        if lifo_reserve is not None and lifo_reserve > 0:
            lifo_reserve_ratio = lifo_reserve / inventory
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="LIFO Reserve Ratio",
                value=lifo_reserve_ratio,
                interpretation=(f"FIFO inventory would be {self.format_percentage(lifo_reserve_ratio)} higher than the "
                                f"reported LIFO amount ({inventory + lifo_reserve:,.0f} vs {inventory:,.0f}); "
                                + ("significant cumulative inflation effect" if lifo_reserve_ratio > 0.2
                                   else "moderate LIFO reserve" if lifo_reserve_ratio > 0.1 else "small LIFO reserve")),
                risk_level=RiskLevel.MODERATE if lifo_reserve_ratio > 0.3 else RiskLevel.LOW,
                methodology="LIFO Reserve / LIFO Inventory; FIFO inventory = LIFO inventory + LIFO reserve",
                limitations=["LIFO reserve represents cumulative impact over multiple periods"]
            ))

        cost_method = self.note_text(notes, 'inventory_method')
        if cost_method:
            assessment = self._assess_method_appropriateness(cost_method, statements)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Inventory Method Assessment",
                value=None,
                interpretation=f"Company uses {cost_method.upper()} - {assessment['assessment']}",
                risk_level=assessment['risk'],
                methodology="Qualitative assessment of inventory method appropriateness",
                limitations=assessment['limitations'],
                label=cost_method
            ))

        return results

    def _analyze_lower_cost_nrv(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Lower of cost and net realizable value"""
        results = []
        notes = statements.notes
        inventory = statements.balance_sheet['inventory']

        inventory_writedown = self.note_number(notes, 'inventory_writedown')
        if inventory_writedown and inventory_writedown > 0:
            writedown_ratio = inventory_writedown / (inventory + inventory_writedown)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Inventory Writedown Impact",
                value=writedown_ratio,
                interpretation=("Significant inventory writedown indicates valuation issues" if writedown_ratio > 0.05
                                else "Moderate inventory adjustment" if writedown_ratio > 0.02 else "Minor inventory writedown"),
                risk_level=RiskLevel.HIGH if writedown_ratio > 0.1 else RiskLevel.MODERATE if writedown_ratio > 0.05 else RiskLevel.LOW,
                methodology="Inventory Writedown / (Inventory + Writedown)",
                limitations=["Writedowns may indicate obsolescence or market decline"]
            ))

        inventory_reserve = self.note_number(notes, 'inventory_obsolescence_reserve')
        if inventory_reserve and inventory_reserve > 0:
            reserve_ratio = inventory_reserve / (inventory + inventory_reserve)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Obsolescence Reserve Ratio",
                value=reserve_ratio,
                interpretation=("High obsolescence reserve suggests inventory quality concerns" if reserve_ratio > 0.1
                                else "Moderate obsolescence provision" if reserve_ratio > 0.05 else "Conservative obsolescence reserve"),
                risk_level=RiskLevel.MODERATE if reserve_ratio > 0.15 else RiskLevel.LOW,
                methodology="Obsolescence Reserve / (Inventory + Reserve)",
                limitations=["Reserve adequacy depends on inventory composition and age"]
            ))

        results.extend(self._assess_nrv_compliance(statements, comparative_data))
        return results

    def _analyze_inflation_impact(self, statements: FinancialStatements,
                                  comparative_data: Optional[List[FinancialStatements]] = None,
                                  industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """LIFO-to-FIFO conversion of COGS and gross margin (CFA):
        FIFO COGS = LIFO COGS - (ending LIFO reserve - beginning LIFO reserve)."""
        results = []
        notes = statements.notes
        income_statement = statements.income_statement
        cost_method = self.note_text(notes, 'inventory_method')
        lifo_reserve = self.note_number(notes, 'lifo_reserve')

        revenue = income_statement.get('revenue')
        cost_of_sales = income_statement.get('cost_of_sales')
        uses_lifo = cost_method == 'lifo' or (lifo_reserve is not None and lifo_reserve > 0)

        if uses_lifo and revenue and revenue > 0 and cost_of_sales is not None:
            reserve_change = self._lifo_reserve_change(statements, comparative_data)
            if reserve_change is None:
                results.append(self.unavailable(
                    AnalysisType.QUALITY, "FIFO-Equivalent Gross Margin Impact",
                    "the change in LIFO reserve is needed (prior-period lifo_reserve in history or notes.prior_lifo_reserve)",
                    "FIFO COGS = LIFO COGS - change in LIFO reserve"))
            else:
                fifo_cogs = cost_of_sales - reserve_change
                margin_impact = (revenue - fifo_cogs) / revenue - (revenue - cost_of_sales) / revenue
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="FIFO-Equivalent Gross Margin Impact",
                    value=margin_impact,
                    interpretation=(f"Under FIFO, cost of sales would be {fifo_cogs:,.0f} instead of {cost_of_sales:,.0f}, "
                                    f"gross margin {self.format_percentage(abs(margin_impact))} "
                                    f"{'higher' if margin_impact > 0 else 'lower' if margin_impact < 0 else 'unchanged'}"),
                    risk_level=RiskLevel.MODERATE if abs(margin_impact) > 0.05 else RiskLevel.LOW,
                    methodology="FIFO gross margin - LIFO gross margin, FIFO COGS = LIFO COGS - change in LIFO reserve",
                    limitations=["Ignores the tax effect of the LIFO/FIFO difference on net income"]
                ))

        inflation_rate = industry_data.get('inflation_rate') if industry_data else None
        if isinstance(inflation_rate, (int, float)) and cost_method in ('fifo', 'lifo'):
            environment = self._determine_economic_environment(float(inflation_rate))
            if environment == EconomicEnvironment.INFLATIONARY:
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Tax Method Efficiency",
                    value=None,
                    interpretation=("LIFO lowers taxable income when prices rise" if cost_method == 'lifo'
                                    else "FIFO results in higher taxable income during inflation (LIFO not permitted under IFRS)"),
                    risk_level=RiskLevel.LOW if cost_method == 'lifo' else RiskLevel.MODERATE,
                    methodology="Qualitative assessment of method choice in an inflationary environment",
                    label=cost_method
                ))

        return results

    def _analyze_inventory_composition(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Raw materials / WIP / finished goods mix (note disclosures)"""
        results = []
        notes = statements.notes
        total_inventory = statements.balance_sheet['inventory']

        components = {
            'raw_materials': self.note_number(notes, 'raw_materials_inventory'),
            'work_in_process': self.note_number(notes, 'wip_inventory'),
            'finished_goods': self.note_number(notes, 'finished_goods_inventory'),
        }
        components = {k: v for k, v in components.items() if v is not None}
        total_components = sum(components.values())
        if not components or total_components <= 0 or abs(total_components - total_inventory) / total_inventory >= 0.1:
            return results

        for component, value in components.items():
            if value > 0:
                component_ratio = value / total_inventory
                component_name = component.replace('_', ' ').title()
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name=f"{component_name} Composition",
                    value=component_ratio,
                    interpretation=f"{component_name} represents {self.format_percentage(component_ratio)} of total inventory",
                    risk_level=RiskLevel.LOW,
                    methodology=f"{component_name} / Total Inventory"
                ))

        wip_ratio = components.get('work_in_process', 0.0) / total_inventory
        finished_goods_ratio = components.get('finished_goods', 0.0) / total_inventory
        if wip_ratio > 0.5:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Inventory Composition Risk",
                value=wip_ratio,
                interpretation="High work-in-process ratio may indicate production inefficiencies",
                risk_level=RiskLevel.MODERATE,
                methodology="Work in Process / Total Inventory"
            ))
        elif finished_goods_ratio > 0.7:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Inventory Composition Risk",
                value=finished_goods_ratio,
                interpretation="High finished goods ratio may indicate demand forecasting issues",
                risk_level=RiskLevel.MODERATE,
                methodology="Finished Goods / Total Inventory"
            ))

        return results

    def _assess_inventory_disclosures(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Disclosure completeness - only assessed when inventory notes were supplied at all"""
        results = []
        notes = statements.notes
        inventory_keys = [k for k in notes if 'inventory' in k or 'lifo' in k or 'nrv' in k or 'obsolescence' in k]
        if not inventory_keys:
            return results

        required_disclosures = {
            'inventory_method': 'Accounting policy for inventory valuation',
            'inventory_composition': 'Breakdown of inventory components',
            'writedown': 'Inventory writedowns',
            'obsolescence': 'Obsolescence evaluation',
        }
        composition_keys = ('raw_materials_inventory', 'wip_inventory', 'finished_goods_inventory')
        disclosure_score = 0
        missing_disclosures = []
        for key, description in required_disclosures.items():
            present = (any(k in notes for k in composition_keys) if key == 'inventory_composition'
                       else any(key in k for k in notes))
            if present:
                disclosure_score += 25
            else:
                missing_disclosures.append(description)

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Inventory Disclosure Quality",
            value=disclosure_score,
            interpretation=("Comprehensive inventory disclosures" if disclosure_score > 75
                            else "Adequate inventory disclosures" if disclosure_score > 50 else "Limited inventory disclosures"),
            risk_level=RiskLevel.LOW if disclosure_score > 75 else RiskLevel.MODERATE if disclosure_score > 50 else RiskLevel.HIGH,
            methodology="Share of the four standard inventory disclosures present in the supplied notes",
            limitations=[f"Missing: {m}" for m in missing_disclosures] or None
        ))
        return results

    def _assess_method_appropriateness(self, method: str, statements: FinancialStatements) -> Dict[
        str, Union[str, RiskLevel, List[str]]]:
        """Assess appropriateness of inventory method choice"""
        business_type = self.note_text(statements.notes, 'business_description')
        method = (method or '').lower()

        if method == 'fifo':
            if 'perishable' in business_type or 'food' in business_type:
                return {'assessment': 'FIFO appropriate for perishable goods business', 'risk': RiskLevel.LOW,
                        'limitations': ['FIFO reflects physical flow for perishables']}
            return {'assessment': 'FIFO provides current cost basis for inventory', 'risk': RiskLevel.LOW,
                    'limitations': ['FIFO may overstate profits during inflation']}
        if method == 'lifo':
            limitations = ['LIFO may understate inventory values']
            if statements.company_info.reporting_standard == ReportingStandard.IFRS:
                return {'assessment': 'LIFO is not permitted under IFRS - check the reporting standard',
                        'risk': RiskLevel.HIGH, 'limitations': limitations}
            return {'assessment': 'LIFO provides tax benefits in inflationary periods', 'risk': RiskLevel.LOW,
                    'limitations': limitations + ['Not permitted under IFRS']}
        if method in ('weighted_average', 'weighted average', 'average', 'avco'):
            return {'assessment': 'Weighted average smooths cost fluctuations', 'risk': RiskLevel.LOW,
                    'limitations': ['May not reflect specific cost identification']}
        return {'assessment': 'Method appropriateness cannot be assessed', 'risk': RiskLevel.MODERATE,
                'limitations': ['Unrecognised inventory method']}

    def _assess_nrv_compliance(self, statements: FinancialStatements,
                               comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """NRV write-downs (only when disclosed)"""
        results = []
        notes = statements.notes
        nrv_writedowns = self.note_number(notes, 'nrv_writedowns') or 0.0
        inventory_impairment = statements.income_statement.get('inventory_impairment', 0.0)
        total_writedowns = abs(nrv_writedowns) + abs(inventory_impairment)

        if total_writedowns > 0:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="NRV Writedown Activity",
                value=total_writedowns,
                interpretation=f"NRV writedowns of {total_writedowns:,.0f} recognised in the period",
                risk_level=RiskLevel.MODERATE,
                methodology="Sum of NRV writedowns and inventory impairments",
                limitations=["Writedowns may indicate market deterioration or obsolescence"]
            ))

        if 'nrv_methodology' in notes:
            disclosed = bool(self.note_text(notes, 'nrv_methodology'))
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="NRV Methodology Disclosure",
                value=None,
                interpretation=("Company discloses its NRV assessment methodology" if disclosed
                                else "NRV methodology note supplied but empty"),
                risk_level=RiskLevel.LOW if disclosed else RiskLevel.MODERATE,
                methodology="Qualitative assessment of NRV disclosure",
                label="disclosed" if disclosed else "not_disclosed"
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

    def get_key_metrics(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        unavailable: Optional[Dict[str, str]] = None) -> Dict[str, float]:
        """Key inventory metrics (average inventory when history is supplied)"""
        inventory = statements.balance_sheet.get('inventory')
        cost_of_sales = statements.income_statement.get('cost_of_sales')
        revenue = statements.income_statement.get('revenue')
        metrics = {}

        if inventory is not None and inventory > 0:
            metrics['inventory_value'] = inventory
            avg_inventory, _ = self.average_balance(statements, comparative_data, 'inventory')
            if cost_of_sales and cost_of_sales > 0:
                metrics['inventory_turnover'] = cost_of_sales / avg_inventory
                metrics['days_inventory_outstanding'] = avg_inventory / cost_of_sales * 365
            if revenue and revenue > 0:
                metrics['inventory_to_sales'] = inventory / revenue
            lifo_reserve = self.note_number(statements.notes, 'lifo_reserve')
            if lifo_reserve and lifo_reserve > 0:
                metrics['lifo_reserve'] = lifo_reserve
                metrics['lifo_reserve_ratio'] = lifo_reserve / inventory

        return metrics

    def create_inventory_efficiency_analysis(self, statements: FinancialStatements,
                                             comparative_data: Optional[
                                                 List[FinancialStatements]] = None) -> InventoryEfficiencyAnalysis:
        """Inventory efficiency object (None where not computable)"""
        inventory = statements.balance_sheet.get('inventory')
        cost_of_sales = statements.income_statement.get('cost_of_sales')
        revenue = statements.income_statement.get('revenue')

        avg_inventory, _ = self.average_balance(statements, comparative_data, 'inventory')
        inventory_turnover = cost_of_sales / avg_inventory if cost_of_sales and avg_inventory else None
        days_inventory_outstanding = avg_inventory / cost_of_sales * 365 if cost_of_sales and avg_inventory else None
        inventory_to_sales_ratio = inventory / revenue if inventory is not None and revenue else None
        inventory_growth_rate = self.growth(inventory, self.prior_value(comparative_data, 'balance_sheet', 'inventory'))

        turnover_trend = TrendDirection.STABLE
        if comparative_data and len(comparative_data) >= 2:
            turnover_values = []
            for p in list(comparative_data) + [statements]:
                inv = p.balance_sheet.get('inventory')
                cogs = p.income_statement.get('cost_of_sales')
                if inv and inv > 0 and cogs and cogs > 0:
                    turnover_values.append(cogs / inv)
            if len(turnover_values) >= 2:
                if turnover_values[-1] > turnover_values[0] * 1.05:
                    turnover_trend = TrendDirection.IMPROVING
                elif turnover_values[-1] < turnover_values[0] * 0.95:
                    turnover_trend = TrendDirection.DETERIORATING

        efficiency_score = None
        if inventory_turnover is not None:
            efficiency_score = 100
            if inventory_turnover < 4:
                efficiency_score -= 20
            if days_inventory_outstanding is not None and days_inventory_outstanding > 90:
                efficiency_score -= 15
            if inventory_to_sales_ratio is not None and inventory_to_sales_ratio > 0.25:
                efficiency_score -= 10

        return InventoryEfficiencyAnalysis(
            inventory_turnover=inventory_turnover,
            days_inventory_outstanding=days_inventory_outstanding,
            inventory_to_sales_ratio=inventory_to_sales_ratio,
            inventory_growth_rate=inventory_growth_rate,
            turnover_trend=turnover_trend,
            efficiency_score=efficiency_score
        )

    def create_inventory_valuation_analysis(self, statements: FinancialStatements) -> InventoryValuationAnalysis:
        """Inventory valuation object"""
        notes = statements.notes
        inventory = statements.balance_sheet.get('inventory', 0.0)
        cost_method_str = self.note_text(notes, 'inventory_method', 'unknown')

        if 'lifo' in cost_method_str:
            cost_method = InventoryMethod.LIFO
        elif 'weighted' in cost_method_str or 'average' in cost_method_str:
            cost_method = InventoryMethod.WEIGHTED_AVERAGE
        elif 'specific' in cost_method_str:
            cost_method = InventoryMethod.SPECIFIC_IDENTIFICATION
        elif 'fifo' in cost_method_str:
            cost_method = InventoryMethod.FIFO
        else:
            cost_method = None

        inventory_reserve = self.note_number(notes, 'inventory_obsolescence_reserve', 0.0)
        lifo_reserve = self.note_number(notes, 'lifo_reserve')

        fifo_equivalent_value = inventory + lifo_reserve if cost_method == InventoryMethod.LIFO and lifo_reserve else None

        quality_score = 100
        obsolescence_indicators = []
        valuation_concerns = []
        if inventory_reserve > 0 and inventory_reserve / (inventory + inventory_reserve) > 0.1:
            quality_score -= 20
            obsolescence_indicators.append("High obsolescence reserve")
        if cost_method == InventoryMethod.LIFO:
            valuation_concerns.append("LIFO may understate current inventory values")
        if cost_method is None:
            valuation_concerns.append("Inventory method not disclosed")

        return InventoryValuationAnalysis(
            cost_method=cost_method,
            current_inventory_value=inventory,
            inventory_reserve=inventory_reserve,
            net_realizable_value=None,  # needs selling prices and costs to complete
            lower_of_cost_nrv=None,
            fifo_equivalent_value=fifo_equivalent_value,
            lifo_equivalent_value=None,
            lifo_reserve=lifo_reserve,
            inventory_quality_score=quality_score,
            obsolescence_indicators=obsolescence_indicators,
            valuation_concerns=valuation_concerns
        )

    def create_inflation_impact_analysis(self, statements: FinancialStatements,
                                         inflation_rate: float = 0.0,
                                         comparative_data: Optional[List[FinancialStatements]] = None
                                         ) -> InflationImpactAnalysis:
        """LIFO-to-FIFO impact object; FIFO COGS = LIFO COGS - change in LIFO reserve."""
        notes = statements.notes
        economic_environment = self._determine_economic_environment(inflation_rate)
        cost_method = self.note_text(notes, 'inventory_method')
        lifo_reserve = self.note_number(notes, 'lifo_reserve')
        revenue = statements.income_statement.get('revenue')
        cost_of_sales = statements.income_statement.get('cost_of_sales')

        fifo_impact_on_cogs = fifo_impact_on_gross_margin = fifo_impact_on_inventory_value = None
        tax_advantage_method = "No significant difference"

        if cost_method == 'lifo' and lifo_reserve is not None:
            fifo_impact_on_inventory_value = lifo_reserve
            reserve_change = self._lifo_reserve_change(statements, comparative_data)
            if reserve_change is not None and revenue and revenue > 0 and cost_of_sales is not None:
                fifo_impact_on_cogs = -reserve_change
                fifo_impact_on_gross_margin = reserve_change / revenue
            if economic_environment == EconomicEnvironment.INFLATIONARY:
                tax_advantage_method = "LIFO provides tax advantage"
        elif cost_method == 'fifo' and economic_environment == EconomicEnvironment.INFLATIONARY:
            tax_advantage_method = "LIFO would provide tax advantage (not used)"

        return InflationImpactAnalysis(
            economic_environment=economic_environment,
            inflation_rate=inflation_rate,
            fifo_impact_on_cogs=fifo_impact_on_cogs,
            fifo_impact_on_gross_margin=fifo_impact_on_gross_margin,
            fifo_impact_on_inventory_value=fifo_impact_on_inventory_value,
            lifo_impact_on_cogs=None,
            lifo_impact_on_gross_margin=None,
            lifo_impact_on_inventory_value=None,
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

    def _lifo_reserve_change(self, statements: FinancialStatements,
                             comparative_data: Optional[List[FinancialStatements]]) -> Optional[float]:
        """Change in LIFO reserve over the period: notes.lifo_reserve_change, else current
        lifo_reserve minus the prior period's (from history or notes.prior_lifo_reserve)."""
        notes = statements.notes
        change = self.note_number(notes, 'lifo_reserve_change')
        if change is not None:
            return change
        current = self.note_number(notes, 'lifo_reserve')
        prior = None
        if comparative_data:
            prior = self.note_number(comparative_data[-1].notes, 'lifo_reserve')
        if prior is None:
            prior = self.note_number(notes, 'prior_lifo_reserve')
        if current is None or prior is None:
            return None
        return current - prior
