
"""Portfolio Portfolio Management Module
===============================

Core portfolio management functionality

===== DATA SOURCES REQUIRED =====
INPUT:
  - Portfolio holdings and transaction history
  - Asset price data and market returns
  - Benchmark indices and market data
  - Investment policy statements and constraints
  - Risk tolerance and preference parameters

OUTPUT:
  - Portfolio performance metrics and attribution
  - Risk analysis and diversification metrics
  - Rebalancing recommendations and optimization
  - Portfolio analytics reports and visualizations
  - Investment strategy recommendations

PARAMETERS:
  - optimization_method: Portfolio optimization method (default: 'mean_variance')
  - risk_free_rate: Risk-free rate for calculations (default: 0.02)
  - rebalance_frequency: Portfolio rebalancing frequency (default: 'quarterly')
  - max_weight: Maximum single asset weight (default: 0.10)
  - benchmark: Portfolio benchmark index (default: 'market_index')
"""



import numpy as np
import pandas as pd
from typing import Dict, List, Optional, Union, Tuple
from dataclasses import dataclass
from enum import Enum
from datetime import datetime, timedelta

from config import AssetClass, MathConstants

class InvestorType(Enum):
    """Types of investors and their characteristics"""
    INDIVIDUAL = "individual"
    INSTITUTIONAL = "institutional"
    PENSION_FUND = "pension_fund"
    ENDOWMENT = "endowment"
    FOUNDATION = "foundation"
    INSURANCE_COMPANY = "insurance_company"
    BANK = "bank"
    SOVEREIGN_WEALTH_FUND = "sovereign_wealth_fund"

class InvestmentObjective(Enum):
    """Primary investment objectives"""
    CAPITAL_PRESERVATION = "capital_preservation"
    CAPITAL_APPRECIATION = "capital_appreciation"
    CURRENT_INCOME = "current_income"
    TOTAL_RETURN = "total_return"

class LifecycleStage(Enum):
    """Individual investor lifecycle stages"""
    ACCUMULATION = "accumulation"
    CONSOLIDATION = "consolidation"
    SPENDING = "spending"
    GIFTING = "gifting"

@dataclass
class InvestorProfile:
    """Comprehensive investor profile"""
    investor_type: InvestorType
    investment_objective: InvestmentObjective
    risk_tolerance: str
    time_horizon: int
    liquidity_needs: float
    tax_situation: Dict
    regulatory_constraints: List[str]
    unique_circumstances: List[str]

class PortfolioApproach:
    """Portfolio approach to investing principles"""

    @staticmethod
    def diversification_benefits() -> Dict:
        """Explain diversification benefits in portfolio approach"""
        return {
            "risk_reduction": {
                "description": "Diversification reduces portfolio risk without proportionally reducing expected return",
                "mathematical_basis": "Portfolio variance < weighted average of individual variances when correlations < 1",
                "optimal_diversification": "Achieved when marginal risk reduction equals marginal cost"
            },
            "correlation_importance": {
                "description": "Lower correlations between assets provide greater diversification benefits",
                "perfect_correlation": "No diversification benefit (ρ = 1)",
                "zero_correlation": "Significant risk reduction possible (ρ = 0)",
                "negative_correlation": "Maximum diversification benefit (ρ < 0)"
            },
            "asset_class_diversification": {
                "description": "Diversification across asset classes provides better risk-return trade-offs",
                "geographic_diversification": "Reduces country-specific risks",
                "currency_diversification": "Reduces currency risk exposure",
                "sector_diversification": "Reduces industry-specific risks"
            }
        }

    @staticmethod
    def portfolio_vs_security_analysis() -> Dict:
        """Compare portfolio approach vs. individual security analysis"""
        return {
            "portfolio_approach": {
                "focus": "Risk-return characteristics of portfolio as a whole",
                "risk_measure": "Portfolio variance and covariances between assets",
                "optimization": "Maximize utility given risk tolerance constraints",
                "diversification": "Central to approach - systematic risk reduction"
            },
            "security_analysis": {
                "focus": "Individual security characteristics and valuation",
                "risk_measure": "Individual security variance and fundamental risks",
                "optimization": "Find undervalued securities with attractive risk-return",
                "diversification": "May be considered but not central focus"
            },
            "integration": {
                "modern_approach": "Combine both approaches for optimal results",
                "top_down": "Asset allocation first, then security selection",
                "bottom_up": "Security selection within asset allocation framework"
            }
        }

class PortfolioManagementProcess:
    """Portfolio management process steps and framework"""

    def __init__(self):
        self.process_steps = [
            "planning", "execution", "feedback"
        ]
        self.planning_substeps = [
            "understand_client_needs", "study_market_conditions",
            "construct_strategic_asset_allocation", "construct_portfolio"
        ]

    def planning_step(self, client_profile: InvestorProfile) -> Dict:
        """Execute planning step of portfolio management process"""
        return {
            "client_needs_analysis": self._analyze_client_needs(client_profile),
            "market_conditions_study": self._study_market_conditions(),
            "strategic_asset_allocation": self._construct_strategic_allocation(client_profile),
            "portfolio_construction_guidelines": self._portfolio_construction_guidelines(client_profile)
        }

    def execution_step(self, allocation_targets: Dict, market_conditions: Dict) -> Dict:
        """Execute execution step of portfolio management process"""
        return {
            "asset_allocation_decisions": self._make_allocation_decisions(allocation_targets),
            "security_selection": self._security_selection_process(allocation_targets),
            "implementation_strategy": self._implementation_strategy(market_conditions),
            "trading_considerations": self._trading_considerations()
        }

    def feedback_step(self, portfolio_performance: Dict, benchmarks: Dict) -> Dict:
        """Execute feedback step of portfolio management process"""
        return {
            "performance_measurement": self._measure_performance(portfolio_performance, benchmarks),
            "performance_evaluation": self._evaluate_performance(portfolio_performance),
            "portfolio_monitoring": self._monitor_portfolio(),
            "rebalancing_needs": self._assess_rebalancing_needs(portfolio_performance)
        }

    def _analyze_client_needs(self, client_profile: InvestorProfile) -> Dict:
        """Analyze client needs and circumstances"""
        return {
            "investment_objectives": {
                "primary_objective": client_profile.investment_objective.value,
                "secondary_objectives": self._identify_secondary_objectives(client_profile),
                "objective_prioritization": self._prioritize_objectives(client_profile)
            },
            "constraints_analysis": {
                "liquidity_constraints": self._analyze_liquidity_needs(client_profile),
                "time_horizon_analysis": self._analyze_time_horizon(client_profile),
                "tax_considerations": self._analyze_tax_situation(client_profile),
                "regulatory_constraints": client_profile.regulatory_constraints,
                "unique_circumstances": client_profile.unique_circumstances
            },
            "risk_analysis": {
                "risk_tolerance_assessment": client_profile.risk_tolerance,
                "risk_capacity_vs_willingness": self._assess_risk_capacity_willingness(client_profile),
                "risk_budget_allocation": self._allocate_risk_budget(client_profile)
            }
        }

    def _study_market_conditions(self) -> Dict:
        """Study current market conditions"""
        return {
            "economic_environment": {
                "gdp_growth": "Current economic growth trends",
                "inflation_expectations": "Inflation outlook and implications",
                "interest_rate_environment": "Current and expected interest rate levels",
                "business_cycle_stage": "Current phase of business cycle"
            },
            "market_conditions": {
                "equity_market_valuation": "Current equity market valuation levels",
                "bond_market_conditions": "Interest rate and credit spread environment",
                "volatility_levels": "Current market volatility and risk premiums",
                "liquidity_conditions": "Market liquidity and trading conditions"
            },
            "outlook_assessment": {
                "short_term_outlook": "1-12 month market outlook",
                "medium_term_outlook": "1-5 year market outlook",
                "long_term_outlook": "5+ year structural trends"
            }
        }

    def _construct_strategic_allocation(self, client_profile: InvestorProfile) -> Dict:
        """Construct strategic asset allocation"""
        # Basic allocation based on investor type and objectives
        if client_profile.investor_type == InvestorType.INDIVIDUAL:
            base_allocation = self._individual_allocation_framework(client_profile)
        elif client_profile.investor_type == InvestorType.PENSION_FUND:
            base_allocation = self._pension_allocation_framework(client_profile)
        elif client_profile.investor_type == InvestorType.ENDOWMENT:
            base_allocation = self._endowment_allocation_framework(client_profile)
        else:
            base_allocation = self._default_allocation_framework(client_profile)

        return {
            "strategic_weights": base_allocation,
            "rebalancing_ranges": self._set_rebalancing_ranges(base_allocation),
            "allocation_rationale": self._explain_allocation_rationale(client_profile, base_allocation),
            "expected_portfolio_characteristics": self._calculate_expected_characteristics(base_allocation)
        }

    def _individual_allocation_framework(self, profile: InvestorProfile) -> Dict[str, float]:
        """Asset allocation framework for individual investors"""
        # Age-based equity allocation rule of thumb: 100 - age = equity %
        if hasattr(profile, 'age'):
            equity_base = max(20, min(80, 100 - profile.age))
        else:
            # Use time horizon as proxy
            equity_base = max(20, min(80, profile.time_horizon * 5))

        # Adjust based on risk tolerance
        risk_adjustments = {
            "conservative": -20,
            "moderate": 0,
            "aggressive": +15
        }
        equity_allocation = equity_base + risk_adjustments.get(profile.risk_tolerance, 0)
        equity_allocation = max(10, min(90, equity_allocation)) / 100

        return {
            "domestic_equity": equity_allocation * 0.6,
            "international_equity": equity_allocation * 0.4,
            "domestic_bonds": (1 - equity_allocation) * 0.7,
            "international_bonds": (1 - equity_allocation) * 0.2,
            "cash": (1 - equity_allocation) * 0.1
        }

    def _pension_allocation_framework(self, profile: InvestorProfile) -> Dict[str, float]:
        """Asset allocation framework for pension funds"""
        return {
            "domestic_equity": 0.35,
            "international_equity": 0.25,
            "domestic_bonds": 0.25,
            "real_estate": 0.10,
            "alternatives": 0.05
        }

    def _endowment_allocation_framework(self, profile: InvestorProfile) -> Dict[str, float]:
        """Asset allocation framework for endowments"""
        return {
            "domestic_equity": 0.25,
            "international_equity": 0.20,
            "domestic_bonds": 0.15,
            "real_estate": 0.15,
            "private_equity": 0.15,
            "hedge_funds": 0.10
        }

    def _default_allocation_framework(self, profile: InvestorProfile) -> Dict[str, float]:
        """Default balanced allocation framework"""
        return {
            "domestic_equity": 0.40,
            "international_equity": 0.20,
            "domestic_bonds": 0.30,
            "cash": 0.10
        }

class InvestorClassification:
    """Classification and characteristics of different investor types"""

    INVESTOR_CHARACTERISTICS = {
        InvestorType.INDIVIDUAL: {
            "typical_objectives": [InvestmentObjective.CAPITAL_APPRECIATION, InvestmentObjective.TOTAL_RETURN],
            "time_horizon": "Variable (accumulation to spending phase)",
            "liquidity_needs": "Generally high",
            "tax_considerations": "Highly relevant",
            "regulatory_constraints": "Minimal",
            "risk_tolerance": "Wide range",
            "typical_constraints": ["Behavioral biases", "Limited investment knowledge", "Tax implications"]
        },
        InvestorType.PENSION_FUND: {
            "typical_objectives": [InvestmentObjective.TOTAL_RETURN],
            "time_horizon": "Long-term",
            "liquidity_needs": "Predictable cash flows",
            "tax_considerations": "Often tax-exempt",
            "regulatory_constraints": "ERISA and other regulations",
            "risk_tolerance": "Moderate to high",
            "typical_constraints": ["Regulatory limits", "Liability matching", "Funding ratio considerations"]
        },
        InvestorType.ENDOWMENT: {
            "typical_objectives": [InvestmentObjective.TOTAL_RETURN],
            "time_horizon": "Perpetual",
            "liquidity_needs": "Annual spending requirements",
            "tax_considerations": "Tax-exempt",
            "regulatory_constraints": "UPMIFA and state regulations",
            "risk_tolerance": "High",
            "typical_constraints": ["Spending policy", "Intergenerational equity", "Mission alignment"]
        },
        InvestorType.INSURANCE_COMPANY: {
            "typical_objectives": [InvestmentObjective.CURRENT_INCOME],
            "time_horizon": "Liability-driven",
            "liquidity_needs": "Claims-driven",
            "tax_considerations": "Corporate tax rates",
            "regulatory_constraints": "Insurance regulations",
            "risk_tolerance": "Conservative",
            "typical_constraints": ["Asset-liability matching", "Regulatory capital", "Credit quality requirements"]
        }
    }

    @classmethod
    def get_investor_characteristics(cls, investor_type: InvestorType) -> Dict:
        """Get characteristics for specific investor type"""
        return cls.INVESTOR_CHARACTERISTICS.get(investor_type, {})

    @classmethod
    def compare_investor_types(cls, investor_types: List[InvestorType]) -> Dict:
        """Compare characteristics across investor types"""
        comparison = {}

        for investor_type in investor_types:
            comparison[investor_type.value] = cls.get_investor_characteristics(investor_type)

        return comparison

    @classmethod
    def lifecycle_analysis(cls, current_stage: LifecycleStage, age: int,
                          financial_situation: Dict) -> Dict:
        """Analyze individual investor lifecycle stage"""
        lifecycle_characteristics = {
            LifecycleStage.ACCUMULATION: {
                "typical_age_range": "20-45",
                "primary_objective": InvestmentObjective.CAPITAL_APPRECIATION,
                "risk_tolerance": "High",
                "time_horizon": "Long",
                "liquidity_needs": "Low",
                "investment_focus": "Growth assets, tax-deferred savings"
            },
            LifecycleStage.CONSOLIDATION: {
                "typical_age_range": "45-60",
                "primary_objective": InvestmentObjective.TOTAL_RETURN,
                "risk_tolerance": "Moderate to High",
                "time_horizon": "Medium to Long",
                "liquidity_needs": "Moderate",
                "investment_focus": "Balanced approach, retirement planning"
            },
            LifecycleStage.SPENDING: {
                "typical_age_range": "60+",
                "primary_objective": InvestmentObjective.CURRENT_INCOME,
                "risk_tolerance": "Conservative to Moderate",
                "time_horizon": "Medium",
                "liquidity_needs": "High",
                "investment_focus": "Income generation, capital preservation"
            },
            LifecycleStage.GIFTING: {
                "typical_age_range": "65+",
                "primary_objective": InvestmentObjective.CAPITAL_APPRECIATION,
                "risk_tolerance": "Varies",
                "time_horizon": "Long (for beneficiaries)",
                "liquidity_needs": "Low",
                "investment_focus": "Growth for next generation, tax efficiency"
            }
        }

        stage_characteristics = lifecycle_characteristics[current_stage]

        # Customize based on individual circumstances
        customizations = {}

        # Age-based adjustments
        if current_stage == LifecycleStage.ACCUMULATION and age > 40:
            customizations["approaching_consolidation"] = True
        elif current_stage == LifecycleStage.CONSOLIDATION and age > 55:
            customizations["approaching_spending"] = True

        # Financial situation adjustments
        if financial_situation.get("wealth_level") == "high":
            customizations["higher_risk_capacity"] = True
        if financial_situation.get("income_stability") == "low":
            customizations["increased_liquidity_needs"] = True

        return {
            "current_stage": current_stage.value,
            "stage_characteristics": stage_characteristics,
            "customizations": customizations,
            "transition_considerations": cls._analyze_transition_needs(current_stage, age, financial_situation)
        }

    @classmethod
    def _analyze_transition_needs(cls, current_stage: LifecycleStage, age: int,
                                financial_situation: Dict) -> Dict:
        """Analyze needs for transitioning between lifecycle stages"""
        transitions = {}

        if current_stage == LifecycleStage.ACCUMULATION:
            years_to_consolidation = max(0, 45 - age)
            transitions["to_consolidation"] = {
                "estimated_years": years_to_consolidation,
                "preparation_needed": years_to_consolidation <= 5,
                "key_actions": ["Increase savings rate", "Diversify investments", "Review risk tolerance"]
            }

        elif current_stage == LifecycleStage.CONSOLIDATION:
            years_to_spending = max(0, 65 - age)
            transitions["to_spending"] = {
                "estimated_years": years_to_spending,
                "preparation_needed": years_to_spending <= 10,
                "key_actions": ["Retirement planning", "Income replacement strategy", "Healthcare planning"]
            }

        return transitions

class PensionPlans:
    """Defined Contribution and Defined Benefit pension plan analysis"""

    @staticmethod
    def defined_contribution_analysis() -> Dict:
        """Analyze DC plan characteristics and investment implications"""
        return {
            "plan_structure": {
                "contribution_mechanism": "Employee and/or employer contributions to individual accounts",
                "investment_responsibility": "Participant bears investment risk and makes investment decisions",
                "benefit_determination": "Benefits based on account balance at retirement",
                "portability": "High - account follows employee"
            },
            "investment_characteristics": {
                "investment_options": "Typically limited menu of mutual funds",
                "asset_allocation_responsibility": "Participant responsibility",
                "lifecycle_funds": "Target-date funds increasingly popular",
                "default_options": "QDIA (Qualified Default Investment Alternative) required"
            },
            "risk_factors": {
                "investment_risk": "Borne entirely by participant",
                "longevity_risk": "Participant bears risk of outliving assets",
                "inflation_risk": "No automatic inflation protection",
                "timing_risk": "Market conditions at retirement impact benefits"
            },
            "advantages": [
                "Portable across employers",
                "Individual control over investments",
                "Potential for higher returns",
                "Transparent account values"
            ],
            "disadvantages": [
                "Investment risk on participant",
                "No guaranteed income",
                "Requires investment knowledge",
                "Vulnerable to market timing"
            ]
        }

    @staticmethod
    def defined_benefit_analysis() -> Dict:
        """Analyze DB plan characteristics and investment implications"""
        return {
            "plan_structure": {
                "benefit_formula": "Predetermined formula based on salary and service",
                "investment_responsibility": "Plan sponsor bears investment risk",
                "benefit_determination": "Formula-driven, typically lifetime annuity",
                "portability": "Limited - benefits tied to specific employer"
            },
            "investment_characteristics": {
                "asset_allocation": "Professional management by plan sponsor",
                "liability_driven_investing": "Assets managed to meet future liabilities",
                "sophisticated_strategies": "Access to alternative investments and advanced strategies",
                "actuarial_assumptions": "Investment returns assumed in actuarial valuations"
            },
            "risk_factors": {
                "sponsor_risk": "Plan dependent on employer's financial health",
                "underfunding_risk": "Plans may become underfunded",
                "regulatory_risk": "Subject to changing regulations",
                "interest_rate_risk": "Liability values sensitive to interest rate changes"
            },
            "advantages": [
                "Guaranteed retirement income",
                "Professional investment management",
                "Pooled longevity risk",
                "Inflation protection (some plans)"
            ],
            "disadvantages": [
                "Limited portability",
                "Dependent on employer viability",
                "Declining availability",
                "Complex regulations"
            ]
        }

    @staticmethod
    def compare_dc_vs_db(participant_profile: Dict) -> Dict:
        """Compare DC vs DB plans for specific participant profile"""
        age = participant_profile.get("age", 35)
        income = participant_profile.get("income", 50000)
        job_mobility = participant_profile.get("job_mobility", "moderate")  # low, moderate, high
        investment_knowledge = participant_profile.get("investment_knowledge", "moderate")

        comparison = {
            "better_for_participant": None,
            "dc_advantages_for_this_profile": [],
            "db_advantages_for_this_profile": [],
            "recommendations": []
        }

        # Age considerations
        if age < 30:
            comparison["dc_advantages_for_this_profile"].append("Long time horizon allows for growth investing")
            comparison["dc_advantages_for_this_profile"].append("High portability valuable for career mobility")
        elif age > 50:
            comparison["db_advantages_for_this_profile"].append("Guaranteed income reduces late-career risk")
            comparison["db_advantages_for_this_profile"].append("Professional management valuable near retirement")

        # Job mobility considerations
        if job_mobility == "high":
            comparison["dc_advantages_for_this_profile"].append("Portability essential for mobile workers")
        elif job_mobility == "low":
            comparison["db_advantages_for_this_profile"].append("Long tenure maximizes DB benefits")

        # Investment knowledge considerations
        if investment_knowledge == "low":
            comparison["db_advantages_for_this_profile"].append("Professional management compensates for lack of knowledge")
            comparison["recommendations"].append("If DC only, consider target-date funds")
        elif investment_knowledge == "high":
            comparison["dc_advantages_for_this_profile"].append("Can potentially outperform through active management")

        # Determine recommendation
        dc_score = len(comparison["dc_advantages_for_this_profile"])
        db_score = len(comparison["db_advantages_for_this_profile"])

        if dc_score > db_score:
            comparison["better_for_participant"] = "DC Plan"
        elif db_score > dc_score:
            comparison["better_for_participant"] = "DB Plan"
        else:
            comparison["better_for_participant"] = "Both have merit"

        return comparison

class AssetManagementIndustry:
    """Asset management industry structure and characteristics"""

    @staticmethod
    def industry_overview() -> Dict:
        """Overview of asset management industry"""
        return {
            "industry_structure": {
                "key_participants": [
                    "Investment management companies",
                    "Banks and trust companies",
                    "Insurance companies",
                    "Brokerage firms",
                    "Independent advisors"
                ],
                "market_segments": [
                    "Institutional asset management",
                    "Retail asset management",
                    "High net worth management",
                    "Alternative investments"
                ],
                "business_models": [
                    "Fee-based management",
                    "Performance-based fees",
                    "Wrap fee programs",
                    "Robo-advisors"
                ]
            },
            "industry_trends": {
                "fee_compression": "Ongoing pressure on management fees",
                "passive_investing_growth": "Continued growth in index funds and ETFs",
                "technology_adoption": "Robo-advisors and digital platforms",
                "regulatory_evolution": "Fiduciary standards and transparency requirements",
                "consolidation": "Industry consolidation through mergers and acquisitions"
            },
            "value_proposition": {
                "professional_management": "Expertise in investment analysis and portfolio construction",
                "diversification": "Access to broad range of investments",
                "economies_of_scale": "Lower costs through pooling",
                "administrative_services": "Record-keeping and reporting services"
            }
        }

    @staticmethod
    def fee_structures() -> Dict:
        """Analysis of asset management fee structures"""
        return {
            "management_fees": {
                "description": "Annual fee based on assets under management",
                "typical_ranges": {
                    "equity_funds": "0.50% - 1.50%",
                    "bond_funds": "0.25% - 1.00%",
                    "index_funds": "0.05% - 0.25%",
                    "alternative_funds": "1.00% - 2.00%"
                },
                "factors_affecting_fees": [
                    "Asset class complexity",
                    "Active vs passive management",
                    "Fund size and scale",
                    "Distribution channels"
                ]
            },
            "performance_fees": {
                "description": "Additional fees based on performance above benchmark",
                "typical_structure": "20% of excess returns above hurdle rate",
                "alignment_benefits": "Aligns manager interests with client outcomes",
                "potential_issues": "Can encourage excessive risk-taking"
            },
            "other_fees": {
                "transaction_costs": "Brokerage and trading costs",
                "administrative_fees": "Custody, record-keeping, and reporting",
                "distribution_fees": "12b-1 fees and sales charges",
                "redemption_fees": "Fees for early withdrawal from funds"
            }
        }

class MutualFunds:
    """Mutual funds and pooled investment products analysis"""

    @staticmethod
    def mutual_fund_structure() -> Dict:
        """Mutual fund structure and characteristics"""
        return {
            "legal_structure": {
                "investment_company": "Regulated under Investment Company Act of 1940",
                "board_of_directors": "Independent oversight of fund operations",
                "investment_advisor": "Manages portfolio and makes investment decisions",
                "fund_shareholders": "Own proportional interest in fund assets"
            },
            "operational_characteristics": {
                "daily_pricing": "NAV calculated daily after market close",
                "liquidity": "Shares redeemable daily at NAV",
                "professional_management": "Full-time portfolio management team",
                "diversification": "Broad diversification within asset class"
            },
            "types_of_funds": {
                "equity_funds": "Invest primarily in stocks",
                "bond_funds": "Invest primarily in fixed-income securities",
                "balanced_funds": "Mix of stocks and bonds",
                "sector_funds": "Focus on specific industry sectors",
                "international_funds": "Invest in foreign securities",
                "index_funds": "Passive replication of market indexes"
            }
        }

    @staticmethod
    def compare_pooled_products() -> Dict:
        """Compare mutual funds with other pooled investment products"""
        return {
            "mutual_funds": {
                "regulation": "Highly regulated under '40 Act",
                "liquidity": "Daily redemption at NAV",
                "transparency": "Daily NAV disclosure, quarterly holdings",
                "minimum_investment": "Typically low ($1,000 - $10,000)",
                "fees": "Management fee + operating expenses",
                "tax_efficiency": "Pass-through taxation, potential for distributions"
            },
            "etfs": {
                "regulation": "Regulated as investment companies or UITs",
                "liquidity": "Intraday trading on exchanges",
                "transparency": "Daily holdings disclosure",
                "minimum_investment": "Cost of one share",
                "fees": "Generally lower expense ratios",
                "tax_efficiency": "More tax efficient due to in-kind redemptions"
            },
            "hedge_funds": {
                "regulation": "Limited regulation, private placements",
                "liquidity": "Limited redemption windows",
                "transparency": "Limited disclosure to investors",
                "minimum_investment": "High ($1M+)",
                "fees": "2 and 20 fee structure typical",
                "tax_efficiency": "Various structures, often pass-through"
            },
            "private_equity": {
                "regulation": "Private placement exemptions",
                "liquidity": "Illiquid, long lock-up periods",
                "transparency": "Quarterly reporting to investors",
                "minimum_investment": "Very high ($5M+)",
                "fees": "Management fee + carried interest",
                "tax_efficiency": "Pass-through with potential tax advantages"
            },
            "unit_investment_trusts": {
                "regulation": "Regulated under '40 Act",
                "liquidity": "Redeemable units, but not actively traded",
                "transparency": "Fixed portfolio disclosed at inception",
                "minimum_investment": "Moderate ($1,000)",
                "fees": "Sales charge + annual fee",
                "tax_efficiency": "Pass-through taxation"
            }
        }

    @staticmethod
    def selection_criteria(investor_profile: InvestorProfile) -> Dict:
        """Provide selection criteria for pooled investment products"""
        recommendations = {
            "primary_recommendations": [],
            "considerations": [],
            "products_to_avoid": []
        }

        # Based on investor type
        if investor_profile.investor_type == InvestorType.INDIVIDUAL:
            if investor_profile.risk_tolerance == "conservative":
                recommendations["primary_recommendations"].extend([
                    "Bond mutual funds",
                    "Balanced funds",
                    "Index funds"
                ])
            elif investor_profile.risk_tolerance == "aggressive":
                recommendations["primary_recommendations"].extend([
                    "Equity mutual funds",
                    "Sector ETFs",
                    "International funds"
                ])

        # Based on liquidity needs
        if investor_profile.liquidity_needs > 0.2:  # High liquidity needs
            recommendations["primary_recommendations"].append("Mutual funds and ETFs")
            recommendations["products_to_avoid"].extend([
                "Private equity",
                "Hedge funds with lock-ups"
            ])

        # Based on investment amount
        if hasattr(investor_profile, 'investment_amount'):
            if investor_profile.investment_amount < 100000:
                recommendations["products_to_avoid"].extend([
                    "Hedge funds",
                    "Private equity"
                ])

        return recommendations

class PortfolioManagement:
    """Main portfolio management interface"""

    def __init__(self):
        self.process = PortfolioManagementProcess()
        self.investor_classification = InvestorClassification()
        self.pension_analysis = PensionPlans()
        self.industry_analysis = AssetManagementIndustry()
        self.mutual_fund_analysis = MutualFunds()

    def comprehensive_portfolio_management_analysis(self, investor_profile: InvestorProfile) -> Dict:
        """Comprehensive portfolio management analysis"""
        return {
            "investor_analysis": {
                "investor_type": investor_profile.investor_type.value,
                "characteristics": self.investor_classification.get_investor_characteristics(investor_profile.investor_type),
                "lifecycle_analysis": self._lifecycle_analysis_if_individual(investor_profile)
            },
            "portfolio_process": {
                "planning": self.process.planning_step(investor_profile),
                "process_overview": self._get_process_overview()
            },
            "product_recommendations": {
                "pooled_products": self.mutual_fund_analysis.selection_criteria(investor_profile),
                "product_comparison": self.mutual_fund_analysis.compare_pooled_products()
            },
            "industry_context": {
                "industry_overview": self.industry_analysis.industry_overview(),
                "fee_analysis": self.industry_analysis.fee_structures()
            }
        }

    def _lifecycle_analysis_if_individual(self, investor_profile: InvestorProfile) -> Optional[Dict]:
        """Perform lifecycle analysis for individual investors"""
        if investor_profile.investor_type == InvestorType.INDIVIDUAL:
            # Estimate lifecycle stage based on available information
            if hasattr(investor_profile, 'age'):
                if investor_profile.age < 45:
                    stage = LifecycleStage.ACCUMULATION
                elif investor_profile.age < 65:
                    stage = LifecycleStage.CONSOLIDATION
                else:
                    stage = LifecycleStage.SPENDING

                return self.investor_classification.lifecycle_analysis(
                    stage, investor_profile.age, {"wealth_level": "moderate"}
                )
        return None

    def _get_process_overview(self) -> Dict:
        """Get overview of portfolio management process"""
        return {
            "process_steps": self.process.process_steps,
            "planning_substeps": self.process.planning_substeps,
            "continuous_nature": "Portfolio management is an ongoing, iterative process",
            "feedback_importance": "Regular monitoring and adjustment essential for success"
        }

    def pension_plan_analysis(self, plan_type: str, participant_profile: Dict) -> Dict:
        """Analyze pension plan characteristics and suitability"""
        if plan_type.lower() == "dc":
            plan_analysis = self.pension_analysis.defined_contribution_analysis()
        elif plan_type.lower() == "db":
            plan_analysis = self.pension_analysis.defined_benefit_analysis()
        else:
            # Compare both
            return {
                "dc_analysis": self.pension_analysis.defined_contribution_analysis(),
                "db_analysis": self.pension_analysis.defined_benefit_analysis(),
                "comparison": self.pension_analysis.compare_dc_vs_db(participant_profile)
            }

        return {
            "plan_analysis": plan_analysis,
            "suitability_for_participant": self._assess_plan_suitability(plan_type, participant_profile)
        }

    def _assess_plan_suitability(self, plan_type: str, participant_profile: Dict) -> Dict:
        """Assess pension plan suitability for specific participant"""
        age = participant_profile.get("age", 35)
        income = participant_profile.get("income", 50000)
        job_mobility = participant_profile.get("job_mobility", "moderate")

        suitability_score = 0
        factors = []

        if plan_type.lower() == "dc":
            # Factors favoring DC plans
            if age < 40:
                suitability_score += 20
                factors.append("Young age favors long-term growth potential")

            if job_mobility == "high":
                suitability_score += 25
                factors.append("High job mobility benefits from portability")

            if participant_profile.get("investment_knowledge", "moderate") == "high":
                suitability_score += 15
                factors.append("Investment knowledge enables active management")

            if income > 75000:
                suitability_score += 10
                factors.append("Higher income allows for greater contributions")

        elif plan_type.lower() == "db":
            # Factors favoring DB plans
            if age > 45:
                suitability_score += 20
                factors.append("Older age benefits from guaranteed income")

            if job_mobility == "low":
                suitability_score += 25
                factors.append("Low job mobility maximizes DB benefit accumulation")

            if participant_profile.get("risk_tolerance", "moderate") == "low":
                suitability_score += 20
                factors.append("Low risk tolerance suits guaranteed benefits")

            if participant_profile.get("investment_knowledge", "moderate") == "low":
                suitability_score += 15
                factors.append("Limited investment knowledge suits professional management")

        return {
            "suitability_score": min(100, suitability_score),
            "suitability_level": "High" if suitability_score > 70 else "Moderate" if suitability_score > 40 else "Low",
            "supporting_factors": factors,
            "recommendations": self._generate_pension_recommendations(plan_type, suitability_score, participant_profile)
        }

    def _generate_pension_recommendations(self, plan_type: str, suitability_score: int,
                                        participant_profile: Dict) -> List[str]:
        """Generate pension plan recommendations"""
        recommendations = []

        if plan_type.lower() == "dc":
            if suitability_score > 70:
                recommendations.append("DC plan well-suited - maximize contributions")
                recommendations.append("Consider aggressive growth allocation if young")
                recommendations.append("Take advantage of employer matching")
            elif suitability_score > 40:
                recommendations.append("DC plan suitable with careful planning")
                recommendations.append("Consider target-date funds for simplicity")
                recommendations.append("Regular portfolio rebalancing important")
            else:
                recommendations.append("DC plan challenges - seek professional guidance")
                recommendations.append("Focus on low-cost index funds")
                recommendations.append("Automate contributions and rebalancing")

        elif plan_type.lower() == "db":
            if suitability_score > 70:
                recommendations.append("DB plan excellent fit - maximize tenure")
                recommendations.append("Understand vesting schedule and benefit formula")
                recommendations.append("Consider supplemental retirement savings")
            elif suitability_score > 40:
                recommendations.append("DB plan provides good foundation")
                recommendations.append("Monitor plan funding status")
                recommendations.append("Diversify with additional retirement accounts")
            else:
                recommendations.append("DB plan may not meet all needs")
                recommendations.append("Supplement with portable retirement savings")
                recommendations.append("Consider career mobility implications")

        return recommendations

    # Helper methods for portfolio management process steps
    def _identify_secondary_objectives(self, profile: InvestorProfile) -> List[str]:
        """Identify secondary investment objectives"""
        secondary = []

        if profile.investment_objective != InvestmentObjective.CAPITAL_PRESERVATION:
            if profile.risk_tolerance == "conservative":
                secondary.append("Capital preservation")

        if profile.investment_objective != InvestmentObjective.CURRENT_INCOME:
            if profile.liquidity_needs > 0.3:
                secondary.append("Current income")

        if profile.investment_objective != InvestmentObjective.CAPITAL_APPRECIATION:
            if profile.time_horizon > 10:
                secondary.append("Capital appreciation")

        return secondary

    def _prioritize_objectives(self, profile: InvestorProfile) -> Dict[str, int]:
        """Prioritize investment objectives"""
        priorities = {
            profile.investment_objective.value: 1
        }

        secondary_objectives = self._identify_secondary_objectives(profile)
        for i, obj in enumerate(secondary_objectives, 2):
            priorities[obj] = i

        return priorities

    def _analyze_liquidity_needs(self, profile: InvestorProfile) -> Dict:
        """Analyze liquidity constraints"""
        return {
            "liquidity_requirement": profile.liquidity_needs,
            "liquidity_level": "High" if profile.liquidity_needs > 0.3 else
                             "Moderate" if profile.liquidity_needs > 0.1 else "Low",
            "liquidity_sources": self._identify_liquidity_sources(profile),
            "emergency_fund_need": max(0.05, profile.liquidity_needs * 1.5)
        }

    def _analyze_time_horizon(self, profile: InvestorProfile) -> Dict:
        """Analyze time horizon constraints"""
        return {
            "time_horizon_years": profile.time_horizon,
            "horizon_category": "Long" if profile.time_horizon > 10 else
                              "Medium" if profile.time_horizon > 5 else "Short",
            "investment_implications": self._time_horizon_implications(profile.time_horizon),
            "stage_transitions": self._identify_stage_transitions(profile)
        }

    def _analyze_tax_situation(self, profile: InvestorProfile) -> Dict:
        """Analyze tax considerations"""
        return {
            "tax_situation": profile.tax_situation,
            "tax_efficiency_importance": "High" if profile.tax_situation.get("marginal_rate", 0) > 0.25 else "Moderate",
            "tax_advantaged_accounts": self._recommend_tax_accounts(profile),
            "tax_loss_harvesting": profile.tax_situation.get("marginal_rate", 0) > 0.15
        }

    def _assess_risk_capacity_willingness(self, profile: InvestorProfile) -> Dict:
        """Assess risk capacity vs willingness"""
        # Simplified risk assessment
        capacity_factors = {
            "time_horizon": min(10, profile.time_horizon) / 10 * 30,
            "liquidity": (1 - profile.liquidity_needs) * 30,
            "income_stability": 20,  # Would need actual data
            "wealth_level": 20       # Would need actual data
        }

        capacity_score = sum(capacity_factors.values())
        willingness_score = {"conservative": 25, "moderate": 50, "aggressive": 85}.get(profile.risk_tolerance, 50)

        return {
            "risk_capacity_score": capacity_score,
            "risk_willingness_score": willingness_score,
            "overall_risk_tolerance": min(capacity_score, willingness_score),
            "capacity_willingness_gap": abs(capacity_score - willingness_score),
            "constraining_factor": "Capacity" if capacity_score < willingness_score else "Willingness"
        }

    def _allocate_risk_budget(self, profile: InvestorProfile) -> Dict:
        """Allocate risk budget across portfolio"""
        risk_assessment = self._assess_risk_capacity_willingness(profile)
        total_risk_budget = risk_assessment["overall_risk_tolerance"]

        if profile.investment_objective == InvestmentObjective.CAPITAL_APPRECIATION:
            allocation = {"equity_risk": 0.7, "credit_risk": 0.2, "other_risk": 0.1}
        elif profile.investment_objective == InvestmentObjective.CURRENT_INCOME:
            allocation = {"credit_risk": 0.6, "equity_risk": 0.3, "other_risk": 0.1}
        else:  # Total return or capital preservation
            allocation = {"equity_risk": 0.5, "credit_risk": 0.4, "other_risk": 0.1}

        return {
            "total_risk_budget": total_risk_budget,
            "risk_allocation": allocation,
            "risk_limits": {risk_type: total_risk_budget * weight
                           for risk_type, weight in allocation.items()}
        }

    def _set_rebalancing_ranges(self, allocation: Dict[str, float]) -> Dict[str, Tuple[float, float]]:
        """Set rebalancing ranges for asset allocation"""
        ranges = {}
        for asset, weight in allocation.items():
            # Wider ranges for smaller allocations
            if weight < 0.1:
                range_width = 0.05
            elif weight < 0.3:
                range_width = 0.07
            else:
                range_width = 0.10

            lower_bound = max(0, weight - range_width)
            upper_bound = min(1, weight + range_width)
            ranges[asset] = (lower_bound, upper_bound)

        return ranges

    def _explain_allocation_rationale(self, profile: InvestorProfile, allocation: Dict[str, float]) -> Dict:
        """Explain rationale for asset allocation"""
        rationale = {
            "primary_drivers": [],
            "risk_considerations": [],
            "return_expectations": [],
            "constraints_addressed": []
        }

        # Primary drivers
        if profile.investment_objective == InvestmentObjective.CAPITAL_APPRECIATION:
            rationale["primary_drivers"].append("Growth-oriented allocation emphasizes equity exposure")
        elif profile.investment_objective == InvestmentObjective.CURRENT_INCOME:
            rationale["primary_drivers"].append("Income-focused allocation emphasizes fixed income")

        # Risk considerations
        if profile.risk_tolerance == "conservative":
            rationale["risk_considerations"].append("Conservative risk tolerance limits equity exposure")
        elif profile.risk_tolerance == "aggressive":
            rationale["risk_considerations"].append("Aggressive risk tolerance allows higher equity allocation")

        # Time horizon
        if profile.time_horizon > 10:
            rationale["return_expectations"].append("Long time horizon supports growth-oriented approach")
        elif profile.time_horizon < 5:
            rationale["return_expectations"].append("Short time horizon requires capital preservation focus")

        # Constraints
        if profile.liquidity_needs > 0.2:
            rationale["constraints_addressed"].append("High liquidity needs addressed through liquid asset allocation")

        return rationale

    def _calculate_expected_characteristics(self, allocation: Dict[str, float]) -> Dict:
        """Calculate expected portfolio characteristics"""
        # Simplified expected returns and risks by asset class
        asset_assumptions = {
            "domestic_equity": {"return": 0.10, "volatility": 0.18},
            "international_equity": {"return": 0.09, "volatility": 0.20},
            "domestic_bonds": {"return": 0.04, "volatility": 0.06},
            "international_bonds": {"return": 0.03, "volatility": 0.08},
            "real_estate": {"return": 0.08, "volatility": 0.15},
            "alternatives": {"return": 0.12, "volatility": 0.25},
            "private_equity": {"return": 0.14, "volatility": 0.30},
            "hedge_funds": {"return": 0.08, "volatility": 0.12},
            "cash": {"return": 0.02, "volatility": 0.01}
        }

        expected_return = 0
        weighted_variance = 0

        for asset, weight in allocation.items():
            if asset in asset_assumptions:
                assumptions = asset_assumptions[asset]
                expected_return += weight * assumptions["return"]
                weighted_variance += (weight * assumptions["volatility"]) ** 2

        # Simplified calculation (assumes zero correlation)
        expected_volatility = np.sqrt(weighted_variance)

        return {
            "expected_annual_return": expected_return,
            "expected_volatility": expected_volatility,
            "expected_sharpe_ratio": (expected_return - 0.03) / expected_volatility if expected_volatility > 0 else 0,
            "risk_return_profile": self._classify_risk_return_profile(expected_return, expected_volatility)
        }

    def _classify_risk_return_profile(self, expected_return: float, expected_volatility: float) -> str:
        """Classify portfolio risk-return profile"""
        if expected_return > 0.08 and expected_volatility > 0.15:
            return "Aggressive Growth"
        elif expected_return > 0.06 and expected_volatility > 0.10:
            return "Moderate Growth"
        elif expected_return > 0.04 and expected_volatility < 0.10:
            return "Conservative Growth"
        elif expected_return < 0.04:
            return "Capital Preservation"
        else:
            return "Balanced"

    # Additional helper methods
    def _identify_liquidity_sources(self, profile: InvestorProfile) -> List[str]:
        """Identify potential liquidity sources"""
        sources = ["Cash and cash equivalents"]

        if profile.liquidity_needs > 0.2:
            sources.extend(["Short-term bond funds", "Money market funds"])

        if profile.time_horizon > 5:
            sources.append("Systematic withdrawal from equity funds")

        return sources

    def _time_horizon_implications(self, time_horizon: int) -> Dict:
        """Analyze investment implications of time horizon"""
        if time_horizon > 10:
            return {
                "asset_allocation": "Can emphasize growth assets",
                "risk_tolerance": "Can accept higher volatility",
                "rebalancing": "Less frequent rebalancing needed",
                "tax_efficiency": "Focus on long-term capital gains"
            }
        elif time_horizon > 5:
            return {
                "asset_allocation": "Balanced approach appropriate",
                "risk_tolerance": "Moderate risk acceptable",
                "rebalancing": "Regular rebalancing important",
                "tax_efficiency": "Consider tax-loss harvesting"
            }
        else:
            return {
                "asset_allocation": "Emphasize capital preservation",
                "risk_tolerance": "Low risk tolerance appropriate",
                "rebalancing": "Frequent monitoring needed",
                "tax_efficiency": "Focus on current income"
            }

    def _identify_stage_transitions(self, profile: InvestorProfile) -> List[str]:
        """Identify upcoming lifecycle stage transitions"""
        transitions = []

        if hasattr(profile, 'age'):
            if 40 <= profile.age <= 50:
                transitions.append("Approaching peak earning years")
            elif 50 <= profile.age <= 60:
                transitions.append("Pre-retirement planning phase")
            elif profile.age > 60:
                transitions.append("Retirement transition")

        return transitions

    def _recommend_tax_accounts(self, profile: InvestorProfile) -> List[str]:
        """Recommend tax-advantaged account types"""
        recommendations = []

        marginal_rate = profile.tax_situation.get("marginal_rate", 0.22)

        if marginal_rate > 0.20:
            recommendations.append("Traditional 401(k) or IRA for current deduction")

        if profile.time_horizon > 10:
            recommendations.append("Roth IRA for tax-free growth")

        if profile.investor_type == InvestorType.INDIVIDUAL:
            recommendations.append("Health Savings Account if eligible")

        return recommendations

    # Portfolio management process helper methods
    def _make_allocation_decisions(self, allocation_targets: Dict) -> Dict:
        """Make tactical allocation decisions"""
        return {
            "strategic_allocation": allocation_targets,
            "tactical_adjustments": "Based on current market conditions",
            "implementation_approach": "Systematic approach to reaching targets",
            "timing_considerations": "Dollar-cost averaging for large allocations"
        }

    def _security_selection_process(self, allocation_targets: Dict) -> Dict:
        """Define security selection process"""
        return {
            "selection_criteria": [
                "Cost efficiency (low expense ratios)",
                "Tracking error minimization for passive funds",
                "Manager tenure and consistency for active funds",
                "Tax efficiency considerations"
            ],
            "due_diligence_process": [
                "Quantitative screening",
                "Qualitative assessment",
                "Risk analysis",
                "Performance attribution"
            ]
        }

    def _implementation_strategy(self, market_conditions: Dict) -> Dict:
        """Define implementation strategy"""
        return {
            "implementation_approach": "Gradual implementation to minimize market impact",
            "cost_management": "Focus on minimizing transaction costs",
            "market_timing": "Avoid market timing, focus on systematic approach",
            "liquidity_management": "Ensure adequate liquidity throughout process"
        }

    def _trading_considerations(self) -> Dict:
        """Define trading considerations"""
        return {
            "execution_priorities": ["Cost minimization", "Market impact reduction", "Speed of execution"],
            "order_types": "Use of limit orders and volume-weighted average price (VWAP)",
            "timing": "Trade during high-liquidity periods when possible",
            "monitoring": "Real-time monitoring of execution quality"
        }

    def _measure_performance(self, portfolio_performance: Dict, benchmarks: Dict) -> Dict:
        """Measure portfolio performance"""
        return {
            "absolute_performance": portfolio_performance.get("total_return", 0),
            "relative_performance": "Performance vs. appropriate benchmarks",
            "risk_adjusted_performance": "Sharpe ratio and other risk-adjusted metrics",
            "attribution_analysis": "Performance attribution by asset class and security selection"
        }

    def _evaluate_performance(self, portfolio_performance: Dict) -> Dict:
        """Evaluate portfolio performance"""
        return {
            "performance_evaluation": "Assessment of returns relative to objectives",
            "risk_evaluation": "Analysis of risk taken relative to risk budget",
            "consistency_evaluation": "Evaluation of performance consistency over time",
            "benchmark_comparison": "Comparison to relevant benchmarks and peer groups"
        }

    def _monitor_portfolio(self) -> Dict:
        """Define portfolio monitoring approach"""
        return {
            "monitoring_frequency": "Continuous monitoring with formal reviews quarterly",
            "key_metrics": ["Asset allocation drift", "Performance vs. benchmarks", "Risk metrics"],
            "alert_systems": "Automated alerts for significant deviations",
            "reporting": "Regular reporting to stakeholders"
        }

    def _assess_rebalancing_needs(self, portfolio_performance: Dict) -> Dict:
        """Assess portfolio rebalancing needs"""
        return {
            "rebalancing_triggers": [
                "Asset allocation drift beyond tolerance bands",
                "Significant market movements",
                "Changes in client circumstances",
                "Calendar-based rebalancing"
            ],
            "rebalancing_approach": "Systematic approach based on predefined rules",
            "cost_benefit_analysis": "Consider transaction costs vs. rebalancing benefits",
            "tax_implications": "Consider tax consequences of rebalancing transactions"
        }