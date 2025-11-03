
"""Portfolio Portfolio Planning Module
===============================

Portfolio planning and strategy development

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
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import Enum

from config import AssetClass, MathConstants
from portfolio_management import InvestorType, InvestmentObjective, InvestorProfile

class ConstraintType(Enum):
    """Types of investment constraints"""
    LIQUIDITY = "liquidity"
    TIME_HORIZON = "time_horizon"
    TAX = "tax"
    LEGAL_REGULATORY = "legal_regulatory"
    UNIQUE_CIRCUMSTANCES = "unique_circumstances"

class ESGApproach(Enum):
    """ESG integration approaches"""
    EXCLUSIONARY = "exclusionary"
    BEST_IN_CLASS = "best_in_class"
    THEMATIC = "thematic"
    INTEGRATION = "integration"
    IMPACT = "impact"
    SHAREHOLDER_ENGAGEMENT = "shareholder_engagement"

@dataclass
class InvestmentPolicyStatement:
    """Investment Policy Statement structure"""
    client_information: Dict
    investment_objectives: Dict
    investment_constraints: Dict
    investment_guidelines: Dict
    strategic_asset_allocation: Dict
    rebalancing_policy: Dict
    performance_measurement: Dict
    responsibilities: Dict
    review_schedule: Dict
    esg_policy: Optional[Dict] = None
    creation_date: datetime = field(default_factory=datetime.now)
    last_review_date: Optional[datetime] = None

class IPSFramework:
    """Investment Policy Statement framework and construction"""

    @staticmethod
    def create_ips_template() -> Dict:
        """Create comprehensive IPS template"""
        return {
            "executive_summary": {
                "purpose": "Define investment objectives, constraints, and guidelines",
                "scope": "Applies to all investment assets under management",
                "governance": "Oversight and decision-making framework"
            },
            "client_information": {
                "client_type": "Individual/Institutional",
                "contact_information": {},
                "background": "Client history and context",
                "financial_situation": "Current financial position"
            },
            "investment_objectives": {
                "return_objectives": {
                    "primary_objective": None,
                    "target_return": None,
                    "return_requirements": [],
                    "benchmark": None
                },
                "risk_objectives": {
                    "risk_tolerance": None,
                    "risk_capacity": None,
                    "risk_constraints": [],
                    "maximum_acceptable_loss": None
                }
            },
            "investment_constraints": {
                "liquidity": {
                    "liquidity_requirements": None,
                    "emergency_fund": None,
                    "cash_flow_needs": []
                },
                "time_horizon": {
                    "investment_horizon": None,
                    "sub_periods": [],
                    "horizon_changes": []
                },
                "tax": {
                    "tax_status": None,
                    "tax_concerns": [],
                    "tax_optimization": []
                },
                "legal_regulatory": {
                    "applicable_regulations": [],
                    "compliance_requirements": [],
                    "prohibited_investments": []
                },
                "unique_circumstances": {
                    "special_considerations": [],
                    "preferences": [],
                    "restrictions": []
                }
            },
            "investment_guidelines": {
                "asset_allocation": {},
                "security_selection": {},
                "portfolio_construction": {},
                "risk_management": {}
            },
            "performance_measurement": {
                "benchmarks": [],
                "evaluation_criteria": [],
                "reporting_frequency": None
            },
            "responsibilities": {
                "client_responsibilities": [],
                "advisor_responsibilities": [],
                "third_party_responsibilities": []
            },
            "review_and_monitoring": {
                "review_frequency": None,
                "review_triggers": [],
                "update_procedures": []
            }
        }

    @staticmethod
    def validate_ips(ips: InvestmentPolicyStatement) -> Dict:
        """Validate IPS completeness and consistency"""
        validation_results = {
            "is_valid": True,
            "errors": [],
            "warnings": [],
            "completeness_score": 0
        }

        # Check required sections
        required_sections = [
            "investment_objectives", "investment_constraints",
            "strategic_asset_allocation", "performance_measurement"
        ]

        for section in required_sections:
            if not hasattr(ips, section) or not getattr(ips, section):
                validation_results["errors"].append(f"Missing required section: {section}")
                validation_results["is_valid"] = False

        # Check objective-constraint consistency
        if hasattr(ips, 'investment_objectives') and hasattr(ips, 'investment_constraints'):
            consistency_check = IPSFramework._check_objective_constraint_consistency(
                ips.investment_objectives, ips.investment_constraints
            )
            validation_results["warnings"].extend(consistency_check)

        # Calculate completeness score
        validation_results["completeness_score"] = IPSFramework._calculate_completeness_score(ips)

        return validation_results

    @staticmethod
    def _check_objective_constraint_consistency(objectives: Dict, constraints: Dict) -> List[str]:
        """Check consistency between objectives and constraints"""
        warnings = []

        # Check return-risk consistency
        if objectives.get("return_objectives", {}).get("target_return", 0) > 0.10:
            if constraints.get("time_horizon", {}).get("investment_horizon", 10) < 5:
                warnings.append("High return target inconsistent with short time horizon")

        # Check liquidity-return consistency
        if constraints.get("liquidity", {}).get("liquidity_requirements", 0) > 0.3:
            if objectives.get("return_objectives", {}).get("target_return", 0) > 0.08:
                warnings.append("High liquidity needs may conflict with return objectives")

        return warnings

    @staticmethod
    def _calculate_completeness_score(ips: InvestmentPolicyStatement) -> float:
        """Calculate IPS completeness score"""
        sections = [
            'client_information', 'investment_objectives', 'investment_constraints',
            'investment_guidelines', 'strategic_asset_allocation', 'rebalancing_policy',
            'performance_measurement', 'responsibilities', 'review_schedule'
        ]

        completed_sections = sum(1 for section in sections
                               if hasattr(ips, section) and getattr(ips, section))

        return (completed_sections / len(sections)) * 100

class ObjectivesFramework:
    """Investment objectives framework and analysis"""

    @staticmethod
    def analyze_return_objectives(client_profile: InvestorProfile,
                                financial_data: Dict) -> Dict:
        """Analyze and set return objectives"""

        # Calculate required return based on goals
        required_return = ObjectivesFramework._calculate_required_return(
            client_profile, financial_data
        )

        # Assess return expectations
        return_expectations = ObjectivesFramework._assess_return_expectations(
            client_profile
        )

        # Set return targets
        return_targets = ObjectivesFramework._set_return_targets(
            required_return, return_expectations, client_profile
        )

        return {
            "required_return": required_return,
            "return_expectations": return_expectations,
            "return_targets": return_targets,
            "return_constraints": ObjectivesFramework._identify_return_constraints(client_profile),
            "benchmark_selection": ObjectivesFramework._select_benchmarks(client_profile)
        }

    @staticmethod
    def analyze_risk_objectives(client_profile: InvestorProfile,
                              return_objectives: Dict) -> Dict:
        """Analyze and set risk objectives"""

        # Assess risk capacity
        risk_capacity = ObjectivesFramework._assess_risk_capacity(client_profile)

        # Assess risk willingness
        risk_willingness = ObjectivesFramework._assess_risk_willingness(client_profile)

        # Reconcile capacity and willingness
        risk_tolerance = ObjectivesFramework._reconcile_risk_tolerance(
            risk_capacity, risk_willingness
        )

        # Set risk constraints
        risk_constraints = ObjectivesFramework._set_risk_constraints(
            risk_tolerance, return_objectives
        )

        return {
            "risk_capacity": risk_capacity,
            "risk_willingness": risk_willingness,
            "overall_risk_tolerance": risk_tolerance,
            "risk_constraints": risk_constraints,
            "risk_metrics": ObjectivesFramework._define_risk_metrics(risk_tolerance)
        }

    @staticmethod
    def _calculate_required_return(client_profile: InvestorProfile,
                                 financial_data: Dict) -> Dict:
        """Calculate required return to meet objectives"""
        current_assets = financial_data.get("current_portfolio_value", 0)
        future_goals = financial_data.get("financial_goals", [])
        annual_contributions = financial_data.get("annual_contributions", 0)

        required_returns = {}

        for goal in future_goals:
            goal_value = goal.get("target_value", 0)
            years_to_goal = goal.get("years_to_goal", 10)

            if current_assets > 0 and years_to_goal > 0:
                # Calculate required return using future value formula
                # FV = PV * (1 + r)^n + PMT * [((1 + r)^n - 1) / r]
                # Solve for r numerically (simplified approach)

                for test_rate in np.arange(0.01, 0.20, 0.001):
                    future_value = (current_assets * (1 + test_rate) ** years_to_goal +
                                  annual_contributions * (((1 + test_rate) ** years_to_goal - 1) / test_rate))

                    if future_value >= goal_value:
                        required_returns[goal.get("name", "goal")] = test_rate
                        break

        return {
            "goal_specific_returns": required_returns,
            "overall_required_return": max(required_returns.values()) if required_returns else 0.07,
            "feasibility_assessment": ObjectivesFramework._assess_return_feasibility(required_returns)
        }

    @staticmethod
    def _assess_return_expectations(client_profile: InvestorProfile) -> Dict:
        """Assess realistic return expectations"""
        # Market-based return expectations by asset class
        market_returns = {
            "cash": 0.02,
            "bonds": 0.04,
            "domestic_equity": 0.09,
            "international_equity": 0.08,
            "real_estate": 0.07,
            "alternatives": 0.10
        }

        # Adjust based on economic environment
        economic_adjustment = 0.0  # Would be based on current conditions

        adjusted_returns = {asset: ret + economic_adjustment
                          for asset, ret in market_returns.items()}

        return {
            "market_based_expectations": adjusted_returns,
            "portfolio_expected_return_range": (0.04, 0.12),
            "risk_adjusted_expectations": ObjectivesFramework._risk_adjust_returns(
                adjusted_returns, client_profile.risk_tolerance
            )
        }

    @staticmethod
    def _set_return_targets(required_return: Dict, expectations: Dict,
                          client_profile: InvestorProfile) -> Dict:
        """Set appropriate return targets"""
        overall_required = required_return.get("overall_required_return", 0.07)
        market_expectation = expectations.get("portfolio_expected_return_range", (0.04, 0.12))

        # Conservative approach: use lower of required and achievable
        if overall_required > market_expectation[1]:
            target_return = market_expectation[1]
            achievability = "Challenging - may require revision of goals"
        elif overall_required < market_expectation[0]:
            target_return = market_expectation[0]
            achievability = "Conservative - goals likely achievable"
        else:
            target_return = overall_required
            achievability = "Reasonable - goals achievable with appropriate risk"

        return {
            "primary_return_target": target_return,
            "return_range": (target_return * 0.8, target_return * 1.2),
            "achievability_assessment": achievability,
            "downside_protection": client_profile.risk_tolerance == "conservative"
        }

    @staticmethod
    def _assess_risk_capacity(client_profile: InvestorProfile) -> Dict:
        """Assess quantitative risk capacity"""
        capacity_factors = {
            "time_horizon": min(1.0, client_profile.time_horizon / 20),
            "liquidity_needs": 1 - client_profile.liquidity_needs,
            "income_stability": 0.8,  # Would be based on actual data
            "wealth_level": 0.7       # Would be based on actual data
        }

        overall_capacity = np.mean(list(capacity_factors.values()))

        return {
            "capacity_score": overall_capacity * 100,
            "capacity_level": "High" if overall_capacity > 0.75 else
                            "Moderate" if overall_capacity > 0.5 else "Low",
            "capacity_factors": capacity_factors,
            "limiting_factors": [factor for factor, score in capacity_factors.items()
                               if score < 0.5]
        }

    @staticmethod
    def _assess_risk_willingness(client_profile: InvestorProfile) -> Dict:
        """Assess behavioral risk willingness"""
        willingness_map = {
            "conservative": 30,
            "moderate": 60,
            "aggressive": 90
        }

        willingness_score = willingness_map.get(client_profile.risk_tolerance, 60)

        return {
            "willingness_score": willingness_score,
            "willingness_level": client_profile.risk_tolerance,
            "behavioral_factors": ObjectivesFramework._identify_behavioral_factors(client_profile),
            "education_needs": willingness_score < 40
        }

    @staticmethod
    def _reconcile_risk_tolerance(capacity: Dict, willingness: Dict) -> Dict:
        """Reconcile risk capacity and willingness"""
        capacity_score = capacity["capacity_score"]
        willingness_score = willingness["willingness_score"]

        # Use more conservative of the two
        overall_tolerance = min(capacity_score, willingness_score)

        mismatch = abs(capacity_score - willingness_score)
        significant_mismatch = mismatch > 30

        return {
            "overall_risk_tolerance": overall_tolerance,
            "risk_level": "High" if overall_tolerance > 75 else
                         "Moderate" if overall_tolerance > 50 else "Low",
            "capacity_willingness_gap": mismatch,
            "significant_mismatch": significant_mismatch,
            "constraining_factor": "Capacity" if capacity_score < willingness_score else "Willingness",
            "reconciliation_approach": ObjectivesFramework._recommend_reconciliation_approach(
                capacity_score, willingness_score, significant_mismatch
            )
        }

class ConstraintsAnalysis:
    """Investment constraints analysis and management"""

    @staticmethod
    def analyze_liquidity_constraints(client_profile: InvestorProfile,
                                    cash_flow_data: Dict) -> Dict:
        """Analyze liquidity requirements and constraints"""

        # Calculate liquidity needs
        liquidity_needs = ConstraintsAnalysis._calculate_liquidity_needs(
            client_profile, cash_flow_data
        )

        # Assess liquidity sources
        liquidity_sources = ConstraintsAnalysis._assess_liquidity_sources(
            cash_flow_data
        )

        # Set liquidity constraints
        liquidity_constraints = ConstraintsAnalysis._set_liquidity_constraints(
            liquidity_needs, liquidity_sources
        )

        return {
            "liquidity_needs": liquidity_needs,
            "liquidity_sources": liquidity_sources,
            "liquidity_constraints": liquidity_constraints,
            "emergency_fund_requirement": ConstraintsAnalysis._calculate_emergency_fund(
                client_profile, cash_flow_data
            )
        }

    @staticmethod
    def analyze_time_horizon_constraints(client_profile: InvestorProfile) -> Dict:
        """Analyze time horizon constraints"""

        primary_horizon = client_profile.time_horizon

        # Identify sub-periods
        sub_periods = ConstraintsAnalysis._identify_sub_periods(client_profile)

        # Asset allocation implications
        allocation_implications = ConstraintsAnalysis._time_horizon_allocation_implications(
            primary_horizon, sub_periods
        )

        return {
            "primary_time_horizon": primary_horizon,
            "horizon_category": "Long" if primary_horizon > 10 else
                              "Medium" if primary_horizon > 5 else "Short",
            "sub_periods": sub_periods,
            "allocation_implications": allocation_implications,
            "glide_path_considerations": ConstraintsAnalysis._assess_glide_path_needs(
                primary_horizon, sub_periods
            )
        }

    @staticmethod
    def analyze_tax_constraints(client_profile: InvestorProfile) -> Dict:
        """Analyze tax considerations and constraints"""

        tax_situation = client_profile.tax_situation

        # Tax efficiency strategies
        tax_strategies = ConstraintsAnalysis._identify_tax_strategies(tax_situation)

        # Account type recommendations
        account_recommendations = ConstraintsAnalysis._recommend_account_types(
            tax_situation, client_profile
        )

        # Asset location strategy
        asset_location = ConstraintsAnalysis._develop_asset_location_strategy(
            tax_situation
        )

        return {
            "tax_situation_analysis": tax_situation,
            "tax_efficiency_strategies": tax_strategies,
            "account_recommendations": account_recommendations,
            "asset_location_strategy": asset_location,
            "tax_loss_harvesting": ConstraintsAnalysis._assess_tax_loss_harvesting(tax_situation)
        }

    @staticmethod
    def analyze_legal_regulatory_constraints(client_profile: InvestorProfile) -> Dict:
        """Analyze legal and regulatory constraints"""

        applicable_regulations = ConstraintsAnalysis._identify_applicable_regulations(
            client_profile
        )

        compliance_requirements = ConstraintsAnalysis._assess_compliance_requirements(
            applicable_regulations, client_profile
        )

        prohibited_investments = ConstraintsAnalysis._identify_prohibited_investments(
            applicable_regulations, client_profile
        )

        return {
            "applicable_regulations": applicable_regulations,
            "compliance_requirements": compliance_requirements,
            "prohibited_investments": prohibited_investments,
            "reporting_obligations": ConstraintsAnalysis._identify_reporting_obligations(
                applicable_regulations
            )
        }

    @staticmethod
    def _calculate_liquidity_needs(client_profile: InvestorProfile,
                                 cash_flow_data: Dict) -> Dict:
        """Calculate specific liquidity needs"""
        annual_expenses = cash_flow_data.get("annual_expenses", 50000)
        irregular_expenses = cash_flow_data.get("irregular_expenses", [])

        # Emergency fund need
        emergency_fund = annual_expenses * 0.5  # 6 months expenses

        # Planned expenses
        planned_expenses = sum(expense.get("amount", 0) for expense in irregular_expenses)

        # Total liquidity need
        total_liquidity = emergency_fund + planned_expenses

        return {
            "emergency_fund_need": emergency_fund,
            "planned_expenses": planned_expenses,
            "total_liquidity_need": total_liquidity,
            "liquidity_percentage": client_profile.liquidity_needs,
            "liquidity_timeline": ConstraintsAnalysis._create_liquidity_timeline(irregular_expenses)
        }

    @staticmethod
    def _identify_sub_periods(client_profile: InvestorProfile) -> List[Dict]:
        """Identify investment sub-periods with different characteristics"""
        sub_periods = []

        if client_profile.investor_type == InvestorType.INDIVIDUAL:
            if hasattr(client_profile, 'age'):
                age = client_profile.age

                if age < 45:
                    sub_periods.append({
                        "period": "Accumulation phase",
                        "years": max(1, 45 - age),
                        "characteristics": "High risk tolerance, growth focus"
                    })

                if 45 <= age < 65:
                    sub_periods.append({
                        "period": "Consolidation phase",
                        "years": max(1, 65 - age),
                        "characteristics": "Moderate risk, balanced approach"
                    })

                if age >= 55:
                    sub_periods.append({
                        "period": "Pre-retirement",
                        "years": max(1, 65 - age),
                        "characteristics": "Risk reduction, income focus"
                    })

        return sub_periods

class AssetAllocationFramework:
    """Asset allocation specification and framework"""

    @staticmethod
    def define_asset_classes(investment_universe: Optional[List[str]] = None) -> Dict:
        """Define and specify asset classes for allocation"""

        if investment_universe is None:
            investment_universe = ["domestic_equity", "international_equity",
                                 "domestic_bonds", "international_bonds",
                                 "real_estate", "commodities", "cash"]

        asset_class_definitions = {}

        for asset_class in investment_universe:
            asset_class_definitions[asset_class] = AssetAllocationFramework._get_asset_class_definition(
                asset_class
            )

        return {
            "asset_classes": asset_class_definitions,
            "correlation_structure": AssetAllocationFramework._estimate_correlation_structure(
                investment_universe
            ),
            "expected_returns": AssetAllocationFramework._estimate_expected_returns(
                investment_universe
            ),
            "risk_characteristics": AssetAllocationFramework._estimate_risk_characteristics(
                investment_universe
            )
        }

    @staticmethod
    def strategic_asset_allocation(client_profile: InvestorProfile,
                                 objectives: Dict,
                                 constraints: Dict,
                                 asset_classes: Dict) -> Dict:
        """Develop strategic asset allocation"""

        # Base allocation by investor type and objectives
        base_allocation = AssetAllocationFramework._get_base_allocation(
            client_profile, objectives
        )

        # Adjust for constraints
        constrained_allocation = AssetAllocationFramework._adjust_for_constraints(
            base_allocation, constraints
        )

        # Optimize allocation
        optimized_allocation = AssetAllocationFramework._optimize_allocation(
            constrained_allocation, asset_classes, objectives
        )

        # Set rebalancing bands
        rebalancing_bands = AssetAllocationFramework._set_rebalancing_bands(
            optimized_allocation
        )

        return {
            "strategic_allocation": optimized_allocation,
            "rebalancing_bands": rebalancing_bands,
            "allocation_rationale": AssetAllocationFramework._explain_allocation_rationale(
                client_profile, objectives, constraints, optimized_allocation
            ),
            "expected_portfolio_characteristics": AssetAllocationFramework._calculate_portfolio_characteristics(
                optimized_allocation, asset_classes
            )
        }

    @staticmethod
    def _get_asset_class_definition(asset_class: str) -> Dict:
        """Get detailed asset class definition"""
        definitions = {
            "domestic_equity": {
                "description": "Domestic equity securities",
                "risk_level": "High",
                "expected_return": 0.09,
                "volatility": 0.18,
                "liquidity": "High",
                "inflation_hedge": "Good"
            },
            "international_equity": {
                "description": "International developed market equity",
                "risk_level": "High",
                "expected_return": 0.08,
                "volatility": 0.20,
                "liquidity": "High",
                "inflation_hedge": "Good"
            },
            "domestic_bonds": {
                "description": "Domestic investment grade bonds",
                "risk_level": "Low to Moderate",
                "expected_return": 0.04,
                "volatility": 0.06,
                "liquidity": "High",
                "inflation_hedge": "Poor"
            },
            "real_estate": {
                "description": "Real estate investment trusts",
                "risk_level": "Moderate",
                "expected_return": 0.07,
                "volatility": 0.15,
                "liquidity": "Moderate",
                "inflation_hedge": "Good"
            },
            "commodities": {
                "description": "Commodity futures and ETFs",
                "risk_level": "High",
                "expected_return": 0.05,
                "volatility": 0.25,
                "liquidity": "Moderate",
                "inflation_hedge": "Excellent"
            },
            "cash": {
                "description": "Cash and cash equivalents",
                "risk_level": "Very Low",
                "expected_return": 0.02,
                "volatility": 0.01,
                "liquidity": "Very High",
                "inflation_hedge": "Poor"
            }
        }

        return definitions.get(asset_class, {})

    @staticmethod
    def _get_base_allocation(client_profile: InvestorProfile, objectives: Dict) -> Dict:
        """Get base asset allocation"""

        if client_profile.investment_objective == InvestmentObjective.CAPITAL_APPRECIATION:
            if client_profile.risk_tolerance == "aggressive":
                return {
                    "domestic_equity": 0.50,
                    "international_equity": 0.30,
                    "domestic_bonds": 0.15,
                    "cash": 0.05
                }
            else:
                return {
                    "domestic_equity": 0.40,
                    "international_equity": 0.20,
                    "domestic_bonds": 0.30,
                    "real_estate": 0.05,
                    "cash": 0.05
                }

        elif client_profile.investment_objective == InvestmentObjective.CURRENT_INCOME:
            return {
                "domestic_bonds": 0.50,
                "domestic_equity": 0.25,
                "real_estate": 0.15,
                "cash": 0.10
            }

        else:  # Total return or balanced
            return {
                "domestic_equity": 0.35,
                "international_equity": 0.15,
                "domestic_bonds": 0.35,
                "real_estate": 0.10,
                "cash": 0.05
            }

class PortfolioConstructionPrinciples:
    """Portfolio construction principles and guidelines"""

    @staticmethod
    def construction_framework() -> Dict:
        """Define portfolio construction framework"""
        return {
            "core_principles": {
                "diversification": "Spread risk across multiple asset classes and securities",
                "cost_efficiency": "Minimize fees and transaction costs",
                "tax_efficiency": "Consider tax implications in all decisions",
                "liquidity_management": "Maintain appropriate liquidity levels",
                "risk_control": "Monitor and control portfolio risk exposures"
            },
            "construction_process": {
                "strategic_allocation": "Set long-term asset class targets",
                "tactical_allocation": "Make short-term adjustments based on market conditions",
                "security_selection": "Choose specific securities within asset classes",
                "implementation": "Execute portfolio construction efficiently",
                "monitoring": "Continuously monitor and rebalance"
            },
            "portfolio_structure": {
                "core_holdings": "Large, stable positions in broad market exposures",
                "satellite_holdings": "Smaller, specialized positions for enhancement",
                "cash_management": "Efficient cash management and transition planning"
            }
        }

    @staticmethod
    def risk_budgeting_approach(total_risk_budget: float,
                               asset_allocation: Dict) -> Dict:
        """Implement risk budgeting approach to portfolio construction"""

        # Estimate risk contributions by asset class
        risk_contributions = PortfolioConstructionPrinciples._estimate_risk_contributions(
            asset_allocation
        )

        # Allocate risk budget
        risk_allocations = {}
        for asset_class, contribution in risk_contributions.items():
            risk_allocations[asset_class] = total_risk_budget * contribution

        return {
            "total_risk_budget": total_risk_budget,
            "risk_contributions": risk_contributions,
            "risk_allocations": risk_allocations,
            "diversification_benefit": PortfolioConstructionPrinciples._calculate_diversification_benefit(
                asset_allocation, risk_contributions
            )
        }

    @staticmethod
    def _estimate_risk_contributions(asset_allocation: Dict) -> Dict:
        """Estimate risk contributions by asset class"""
        # Simplified risk contribution estimation
        risk_weights = {
            "domestic_equity": 0.18,
            "international_equity": 0.20,
            "domestic_bonds": 0.06,
            "international_bonds": 0.08,
            "real_estate": 0.15,
            "commodities": 0.25,
            "cash": 0.01
        }

        contributions = {}
        total_risk = 0

        # Calculate weighted risk
        for asset_class, weight in asset_allocation.items():
            if asset_class in risk_weights:
                risk_contribution = weight * risk_weights[asset_class]
                contributions[asset_class] = risk_contribution
                total_risk += risk_contribution

        # Normalize to sum to 1
        if total_risk > 0:
            contributions = {k: v / total_risk for k, v in contributions.items()}

        return contributions

class ESGIntegration:
    """Environmental, Social, and Governance integration framework"""

    @staticmethod
    def esg_integration_approaches() -> Dict:
        """Define ESG integration approaches"""
        return {
            ESGApproach.EXCLUSIONARY.value: {
                "description": "Exclude investments based on ESG criteria",
                "implementation": "Screen out tobacco, weapons, fossil fuels, etc.",
                "pros": ["Clear ethical alignment", "Simple to implement"],
                "cons": ["May reduce diversification", "Potential return impact"],
                "suitable_for": "Values-driven investors with specific exclusions"
            },
            ESGApproach.BEST_IN_CLASS.value: {
                "description": "Select best ESG performers within each sector",
                "implementation": "Choose top ESG-rated companies in each industry",
                "pros": ["Maintains sector diversification", "Potential for outperformance"],
                "cons": ["May include controversial sectors", "Complex evaluation process"],
                "suitable_for": "Investors seeking ESG improvement without major exclusions"
            },
            ESGApproach.THEMATIC.value: {
                "description": "Invest in themes aligned with sustainable development",
                "implementation": "Focus on clean energy, water, healthcare, education",
                "pros": ["Positive impact potential", "Growth opportunity exposure"],
                "cons": ["Concentration risk", "Potential volatility"],
                "suitable_for": "Investors targeting specific sustainability themes"
            },
            ESGApproach.INTEGRATION.value: {
                "description": "Incorporate ESG factors into traditional analysis",
                "implementation": "ESG factors as part of fundamental analysis",
                "pros": ["Comprehensive risk assessment", "Potential alpha generation"],
                "cons": ["Complex implementation", "Requires ESG expertise"],
                "suitable_for": "Sophisticated investors seeking enhanced risk-return"
            },
            ESGApproach.IMPACT.value: {
                "description": "Target measurable positive social/environmental impact",
                "implementation": "Direct investment in solutions with impact measurement",
                "pros": ["Measurable positive outcomes", "Mission alignment"],
                "cons": ["Limited investment universe", "Potential return trade-offs"],
                "suitable_for": "Impact-focused investors with specific outcome goals"
            },
            ESGApproach.SHAREHOLDER_ENGAGEMENT.value: {
                "description": "Active ownership to influence corporate ESG practices",
                "implementation": "Proxy voting, shareholder resolutions, management dialogue",
                "pros": ["Influence corporate behavior", "Maintain diversification"],
                "cons": ["Requires active management", "Uncertain outcomes"],
                "suitable_for": "Large investors with capacity for active engagement"
            }
        }

    @staticmethod
    def develop_esg_policy(client_profile: InvestorProfile,
                          esg_preferences: Dict) -> Dict:
        """Develop ESG policy for portfolio"""

        # Assess ESG priorities
        esg_priorities = ESGIntegration._assess_esg_priorities(esg_preferences)

        # Select appropriate approaches
        recommended_approaches = ESGIntegration._select_esg_approaches(
            esg_priorities, client_profile
        )

        # Develop implementation strategy
        implementation_strategy = ESGIntegration._develop_implementation_strategy(
            recommended_approaches, client_profile
        )

        return {
            "esg_priorities": esg_priorities,
            "recommended_approaches": recommended_approaches,
            "implementation_strategy": implementation_strategy,
            "measurement_framework": ESGIntegration._create_measurement_framework(
                recommended_approaches
            ),
            "reporting_requirements": ESGIntegration._define_reporting_requirements(
                recommended_approaches
            )
        }

    @staticmethod
    def _assess_esg_priorities(esg_preferences: Dict) -> Dict:
        """Assess client ESG priorities"""
        environmental_weight = esg_preferences.get("environmental_importance", 5) / 10
        social_weight = esg_preferences.get("social_importance", 5) / 10
        governance_weight = esg_preferences.get("governance_importance", 5) / 10

        return {
            "environmental_weight": environmental_weight,
            "social_weight": social_weight,
            "governance_weight": governance_weight,
            "primary_focus": max([
                ("environmental", environmental_weight),
                ("social", social_weight),
                ("governance", governance_weight)
            ], key=lambda x: x[1])[0],
            "overall_esg_importance": np.mean([environmental_weight, social_weight, governance_weight])
        }

    @staticmethod
    def _select_esg_approaches(esg_priorities: Dict, client_profile: InvestorProfile) -> List[str]:
        """Select appropriate ESG approaches"""
        approaches = []

        esg_importance = esg_priorities["overall_esg_importance"]

        # High ESG importance
        if esg_importance > 0.7:
            if client_profile.investment_objective == InvestmentObjective.CAPITAL_APPRECIATION:
                approaches.extend([ESGApproach.THEMATIC.value, ESGApproach.INTEGRATION.value])
            else:
                approaches.extend([ESGApproach.BEST_IN_CLASS.value, ESGApproach.INTEGRATION.value])

        # Moderate ESG importance
        elif esg_importance > 0.4:
            approaches.append(ESGApproach.INTEGRATION.value)
            if "exclusions" in client_profile.unique_circumstances:
                approaches.append(ESGApproach.EXCLUSIONARY.value)

        # Low ESG importance
        else:
            approaches.append(ESGApproach.INTEGRATION.value)

        return approaches

class PortfolioPlanning:
    """Main portfolio planning and construction interface"""

    def __init__(self):
        self.ips_framework = IPSFramework()
        self.objectives_framework = ObjectivesFramework()
        self.constraints_analysis = ConstraintsAnalysis()
        self.asset_allocation_framework = AssetAllocationFramework()
        self.construction_principles = PortfolioConstructionPrinciples()
        self.esg_integration = ESGIntegration()

    def create_comprehensive_ips(self, client_profile: InvestorProfile,
                                financial_data: Dict,
                                esg_preferences: Optional[Dict] = None) -> InvestmentPolicyStatement:
        """Create comprehensive Investment Policy Statement"""

        # Analyze objectives
        return_objectives = self.objectives_framework.analyze_return_objectives(
            client_profile, financial_data
        )
        risk_objectives = self.objectives_framework.analyze_risk_objectives(
            client_profile, return_objectives
        )

        # Analyze constraints
        liquidity_constraints = self.constraints_analysis.analyze_liquidity_constraints(
            client_profile, financial_data
        )
        time_horizon_constraints = self.constraints_analysis.analyze_time_horizon_constraints(
            client_profile
        )
        tax_constraints = self.constraints_analysis.analyze_tax_constraints(client_profile)
        legal_constraints = self.constraints_analysis.analyze_legal_regulatory_constraints(
            client_profile
        )

        # Develop asset allocation
        asset_classes = self.asset_allocation_framework.define_asset_classes()
        strategic_allocation = self.asset_allocation_framework.strategic_asset_allocation(
            client_profile,
            {"return_objectives": return_objectives, "risk_objectives": risk_objectives},
            {
                "liquidity": liquidity_constraints,
                "time_horizon": time_horizon_constraints,
                "tax": tax_constraints,
                "legal": legal_constraints
            },
            asset_classes
        )

        # ESG policy if applicable
        esg_policy = None
        if esg_preferences:
            esg_policy = self.esg_integration.develop_esg_policy(
                client_profile, esg_preferences
            )

        # Create IPS
        ips = InvestmentPolicyStatement(
            client_information=self._compile_client_information(client_profile, financial_data),
            investment_objectives={
                "return_objectives": return_objectives,
                "risk_objectives": risk_objectives
            },
            investment_constraints={
                "liquidity": liquidity_constraints,
                "time_horizon": time_horizon_constraints,
                "tax": tax_constraints,
                "legal_regulatory": legal_constraints,
                "unique_circumstances": self._analyze_unique_circumstances(client_profile)
            },
            investment_guidelines=self._develop_investment_guidelines(
                strategic_allocation, client_profile
            ),
            strategic_asset_allocation=strategic_allocation,
            rebalancing_policy=self._develop_rebalancing_policy(strategic_allocation),
            performance_measurement=self._develop_performance_measurement_framework(
                return_objectives, strategic_allocation
            ),
            responsibilities=self._define_responsibilities(),
            review_schedule=self._establish_review_schedule(client_profile),
            esg_policy=esg_policy
        )

        return ips

    def validate_and_optimize_ips(self, ips: InvestmentPolicyStatement) -> Dict:
        """Validate and provide optimization recommendations for IPS"""

        # Validate IPS
        validation_results = self.ips_framework.validate_ips(ips)

        # Optimization recommendations
        optimization_recommendations = self._generate_optimization_recommendations(ips)

        # Stress test the plan
        stress_test_results = self._stress_test_ips(ips)

        return {
            "validation_results": validation_results,
            "optimization_recommendations": optimization_recommendations,
            "stress_test_results": stress_test_results,
            "implementation_roadmap": self._create_implementation_roadmap(ips)
        }

    def portfolio_construction_analysis(self, ips: InvestmentPolicyStatement,
                                      market_conditions: Dict) -> Dict:
        """Comprehensive portfolio construction analysis"""

        # Construction framework
        construction_framework = self.construction_principles.construction_framework()

        # Risk budgeting
        risk_budget_analysis = self.construction_principles.risk_budgeting_approach(
            ips.investment_objectives["risk_objectives"]["overall_risk_tolerance"]["overall_risk_tolerance"],
            ips.strategic_asset_allocation["strategic_allocation"]
        )

        # Implementation considerations
        implementation_analysis = self._analyze_implementation_considerations(
            ips, market_conditions
        )

        # Monitoring framework
        monitoring_framework = self._develop_monitoring_framework(ips)

        return {
            "construction_framework": construction_framework,
            "risk_budget_analysis": risk_budget_analysis,
            "implementation_analysis": implementation_analysis,
            "monitoring_framework": monitoring_framework,
            "success_metrics": self._define_success_metrics(ips)
        }

    def _compile_client_information(self, client_profile: InvestorProfile,
                                  financial_data: Dict) -> Dict:
        """Compile comprehensive client information"""
        return {
            "client_type": client_profile.investor_type.value,
            "investment_objective": client_profile.investment_objective.value,
            "risk_tolerance": client_profile.risk_tolerance,
            "time_horizon": client_profile.time_horizon,
            "liquidity_needs": client_profile.liquidity_needs,
            "financial_situation": {
                "current_assets": financial_data.get("current_portfolio_value", 0),
                "annual_income": financial_data.get("annual_income", 0),
                "annual_expenses": financial_data.get("annual_expenses", 0),
                "net_worth": financial_data.get("net_worth", 0)
            },
            "tax_situation": client_profile.tax_situation,
            "unique_circumstances": client_profile.unique_circumstances
        }

    def _analyze_unique_circumstances(self, client_profile: InvestorProfile) -> Dict:
        """Analyze unique circumstances affecting portfolio"""
        unique_analysis = {
            "circumstances": client_profile.unique_circumstances,
            "portfolio_implications": [],
            "special_considerations": []
        }

        for circumstance in client_profile.unique_circumstances:
            if "concentrated position" in circumstance.lower():
                unique_analysis["portfolio_implications"].append(
                    "Diversification challenge due to concentrated position"
                )
                unique_analysis["special_considerations"].append(
                    "Consider gradual diversification strategy"
                )

            elif "business ownership" in circumstance.lower():
                unique_analysis["portfolio_implications"].append(
                    "High correlation between human capital and financial capital"
                )
                unique_analysis["special_considerations"].append(
                    "Emphasize diversification away from business sector"
                )

            elif "inheritance" in circumstance.lower():
                unique_analysis["special_considerations"].append(
                    "Consider step-up in basis for tax planning"
                )

        return unique_analysis

    def _develop_investment_guidelines(self, strategic_allocation: Dict,
                                     client_profile: InvestorProfile) -> Dict:
        """Develop comprehensive investment guidelines"""
        return {
            "asset_allocation_guidelines": {
                "strategic_targets": strategic_allocation["strategic_allocation"],
                "rebalancing_bands": strategic_allocation["rebalancing_bands"],
                "tactical_ranges": self._set_tactical_ranges(strategic_allocation)
            },
            "security_selection_guidelines": {
                "quality_requirements": self._define_quality_requirements(client_profile),
                "diversification_requirements": self._define_diversification_requirements(),
                "liquidity_requirements": self._define_liquidity_requirements(client_profile),
                "cost_guidelines": self._define_cost_guidelines()
            },
            "risk_management_guidelines": {
                "maximum_position_sizes": self._set_position_limits(),
                "prohibited_investments": self._identify_prohibited_investments(client_profile),
                "derivative_usage": self._define_derivative_usage_policy(client_profile)
            }
        }

    def _develop_rebalancing_policy(self, strategic_allocation: Dict) -> Dict:
        """Develop rebalancing policy"""
        return {
            "rebalancing_method": "Threshold-based with calendar review",
            "rebalancing_bands": strategic_allocation["rebalancing_bands"],
            "rebalancing_frequency": {
                "calendar_review": "Quarterly",
                "threshold_monitoring": "Monthly",
                "emergency_rebalancing": "As needed for major market events"
            },
            "rebalancing_priorities": [
                "Bring severely out-of-range allocations back to target",
                "Consider tax implications of rebalancing transactions",
                "Use cash flows to rebalance when possible",
                "Minimize transaction costs"
            ],
            "implementation_guidelines": {
                "minimum_trade_size": "1% of portfolio value",
                "tax_loss_harvesting": "Incorporate when beneficial",
                "cash_flow_utilization": "Use contributions/withdrawals for rebalancing"
            }
        }

    def _develop_performance_measurement_framework(self, return_objectives: Dict,
                                                  strategic_allocation: Dict) -> Dict:
        """Develop performance measurement framework"""
        return {
            "primary_benchmark": self._select_primary_benchmark(strategic_allocation),
            "secondary_benchmarks": self._select_secondary_benchmarks(strategic_allocation),
            "performance_metrics": [
                "Total return vs. benchmark",
                "Risk-adjusted returns (Sharpe ratio)",
                "Maximum drawdown",
                "Tracking error vs. benchmark"
            ],
            "evaluation_periods": {
                "short_term": "1 year",
                "medium_term": "3 years",
                "long_term": "5+ years"
            },
            "performance_attribution": {
                "asset_allocation_effect": "Contribution from strategic allocation decisions",
                "security_selection_effect": "Contribution from security selection",
                "interaction_effect": "Interaction between allocation and selection"
            },
            "reporting_schedule": {
                "monthly": "Portfolio value and basic performance metrics",
                "quarterly": "Comprehensive performance report with attribution",
                "annual": "Full performance review with recommendations"
            }
        }

    def _define_responsibilities(self) -> Dict:
        """Define roles and responsibilities"""
        return {
            "client_responsibilities": [
                "Provide accurate and complete financial information",
                "Communicate changes in circumstances promptly",
                "Review and approve Investment Policy Statement",
                "Make timely decisions on recommended changes"
            ],
            "advisor_responsibilities": [
                "Develop and maintain Investment Policy Statement",
                "Implement investment strategy according to IPS",
                "Monitor portfolio performance and risk",
                "Provide regular reporting and communication",
                "Recommend changes when appropriate"
            ],
            "third_party_responsibilities": [
                "Custodian: Safekeeping of assets and transaction settlement",
                "Portfolio managers: Investment management within guidelines",
                "Other service providers: Specific services as contracted"
            ]
        }

    def _establish_review_schedule(self, client_profile: InvestorProfile) -> Dict:
        """Establish IPS and portfolio review schedule"""
        if client_profile.investor_type == InvestorType.INDIVIDUAL:
            review_frequency = "Annual"
            interim_reviews = "As circumstances change"
        else:
            review_frequency = "Annual or as required by governance"
            interim_reviews = "Quarterly committee reviews"

        return {
            "formal_review_frequency": review_frequency,
            "interim_review_triggers": [
                "Significant changes in client circumstances",
                "Major market events or economic changes",
                "Performance significantly off-track",
                "Changes in investment objectives or constraints"
            ],
            "review_process": {
                "preparation": "Gather performance data and market analysis",
                "review_meeting": "Discuss performance, circumstances, and changes",
                "documentation": "Update IPS if changes are made",
                "implementation": "Execute any approved changes"
            },
            "update_procedures": [
                "Document reasons for any IPS changes",
                "Obtain client approval for material changes",
                "Communicate changes to all relevant parties",
                "Update systems and processes accordingly"
            ]
        }

    def _generate_optimization_recommendations(self, ips: InvestmentPolicyStatement) -> List[str]:
        """Generate IPS optimization recommendations"""
        recommendations = []

        # Check asset allocation efficiency
        allocation = ips.strategic_asset_allocation.get("strategic_allocation", {})
        if len(allocation) < 4:
            recommendations.append("Consider additional asset classes for better diversification")

        # Check rebalancing policy
        if not ips.rebalancing_policy.get("rebalancing_bands"):
            recommendations.append("Establish clear rebalancing bands to maintain strategic allocation")

        # Check performance measurement
        if not ips.performance_measurement.get("primary_benchmark"):
            recommendations.append("Define clear performance benchmarks for evaluation")

        # Check ESG integration
        if not ips.esg_policy and "esg" in str(ips.client_information.get("unique_circumstances", [])).lower():
            recommendations.append("Consider developing ESG policy based on client preferences")

        return recommendations

    def _stress_test_ips(self, ips: InvestmentPolicyStatement) -> Dict:
        """Stress test the IPS under various scenarios"""
        allocation = ips.strategic_asset_allocation.get("strategic_allocation", {})

        # Scenario analysis
        scenarios = {
            "market_crash": {"equity_shock": -0.30, "bond_shock": 0.05},
            "inflation_spike": {"equity_shock": -0.10, "bond_shock": -0.15},
            "recession": {"equity_shock": -0.20, "bond_shock": 0.10}
        }

        stress_results = {}
        for scenario_name, shocks in scenarios.items():
            portfolio_impact = 0
            for asset_class, weight in allocation.items():
                if "equity" in asset_class:
                    portfolio_impact += weight * shocks["equity_shock"]
                elif "bond" in asset_class:
                    portfolio_impact += weight * shocks["bond_shock"]

            stress_results[scenario_name] = {
                "portfolio_impact": portfolio_impact,
                "severity": "High" if abs(portfolio_impact) > 0.15 else
                          "Moderate" if abs(portfolio_impact) > 0.10 else "Low"
            }

        return {
            "scenario_analysis": stress_results,
            "overall_resilience": "Good" if all(abs(result["portfolio_impact"]) < 0.20
                                              for result in stress_results.values()) else "Moderate",
            "recommendations": self._generate_stress_test_recommendations(stress_results)
        }

    def _create_implementation_roadmap(self, ips: InvestmentPolicyStatement) -> Dict:
        """Create implementation roadmap for IPS"""
        return {
            "phase_1_immediate": {
                "timeframe": "0-30 days",
                "tasks": [
                    "Finalize and approve IPS",
                    "Set up custodial and administrative accounts",
                    "Implement core strategic allocation"
                ]
            },
            "phase_2_buildup": {
                "timeframe": "30-90 days",
                "tasks": [
                    "Complete portfolio construction",
                    "Implement security selection",
                    "Establish monitoring and reporting systems"
                ]
            },
            "phase_3_optimization": {
                "timeframe": "90+ days",
                "tasks": [
                    "Fine-tune allocation based on performance",
                    "Optimize tax efficiency",
                    "Conduct first quarterly review"
                ]
            }
        }

    # Additional helper methods for portfolio construction analysis
    def _analyze_implementation_considerations(self, ips: InvestmentPolicyStatement,
                                             market_conditions: Dict) -> Dict:
        """Analyze implementation considerations"""
        return {
            "market_timing_considerations": {
                "current_valuations": market_conditions.get("market_valuations", "neutral"),
                "volatility_environment": market_conditions.get("volatility", "normal"),
                "implementation_approach": "Dollar-cost averaging for large allocations"
            },
            "cost_analysis": {
                "estimated_implementation_costs": "0.10% - 0.25% of assets",
                "ongoing_management_fees": "0.50% - 1.50% annually",
                "transaction_cost_minimization": "Use low-cost index funds where appropriate"
            },
            "tax_optimization": {
                "account_type_utilization": "Maximize tax-advantaged account usage",
                "asset_location": "Place tax-inefficient assets in tax-advantaged accounts",
                "transition_management": "Consider tax implications of portfolio transitions"
            }
        }

    def _develop_monitoring_framework(self, ips: InvestmentPolicyStatement) -> Dict:
        """Develop comprehensive monitoring framework"""
        return {
            "daily_monitoring": [
                "Portfolio value and performance",
                "Cash flows and liquidity",
                "Market risk exposures"
            ],
            "monthly_monitoring": [
                "Asset allocation drift",
                "Performance vs. benchmarks",
                "Risk metrics and attribution"
            ],
            "quarterly_monitoring": [
                "Comprehensive performance review",
                "Rebalancing needs assessment",
                "Strategy effectiveness evaluation"
            ],
            "alert_systems": {
                "allocation_alerts": "Trigger when allocation exceeds bands",
                "performance_alerts": "Trigger on significant underperformance",
                "risk_alerts": "Trigger on excessive risk measures"
            }
        }

    def _define_success_metrics(self, ips: InvestmentPolicyStatement) -> Dict:
        """Define success metrics for portfolio"""
        return_target = ips.investment_objectives["return_objectives"]["return_targets"]["primary_return_target"]

        return {
            "primary_success_metrics": {
                "return_achievement": f"Achieve {return_target:.1%} annual return over long term",
                "risk_control": "Stay within defined risk parameters",
                "objective_fulfillment": "Meet stated investment objectives"
            },
            "secondary_success_metrics": {
                "cost_efficiency": "Minimize total investment costs",
                "tax_efficiency": "Optimize after-tax returns",
                "implementation_efficiency": "Minimize tracking error to strategic allocation"
            },
            "measurement_timeframes": {
                "short_term": "1-year rolling periods",
                "medium_term": "3-year rolling periods",
                "long_term": "5+ year periods"
            }
        }

    # Helper methods for guidelines and policies
    def _set_tactical_ranges(self, strategic_allocation: Dict) -> Dict:
        """Set tactical allocation ranges"""
        tactical_ranges = {}
        for asset_class, target in strategic_allocation["strategic_allocation"].items():
            # Allow 25% tactical deviation from strategic target
            deviation = target * 0.25
            tactical_ranges[asset_class] = {
                "minimum": max(0, target - deviation),
                "maximum": min(1, target + deviation)
            }
        return tactical_ranges

    def _define_quality_requirements(self, client_profile: InvestorProfile) -> List[str]:
        """Define security quality requirements"""
        requirements = ["Minimum investment grade rating for fixed income"]

        if client_profile.risk_tolerance == "conservative":
            requirements.extend([
                "Large-cap equity bias",
                "Minimum market capitalization of $2 billion for individual stocks"
            ])

        return requirements

    def _define_diversification_requirements(self) -> List[str]:
        """Define diversification requirements"""
        return [
            "Maximum 5% in any single security",
            "Maximum 25% in any single sector",
            "Minimum 20 individual securities in equity allocation"
        ]

    def _define_liquidity_requirements(self, client_profile: InvestorProfile) -> List[str]:
        """Define liquidity requirements"""
        requirements = ["Minimum daily trading volume of $1 million for individual securities"]

        if client_profile.liquidity_needs > 0.2:
            requirements.append("Maximum 10% in illiquid investments")
        else:
            requirements.append("Maximum 20% in illiquid investments")

        return requirements

    def _define_cost_guidelines(self) -> List[str]:
        """Define cost guidelines"""
        return [
            "Target expense ratios below 0.75% for actively managed funds",
            "Target expense ratios below 0.25% for index funds",
            "Minimize portfolio turnover to reduce transaction costs"
        ]

    def _set_position_limits(self) -> Dict[str, float]:
        """Set maximum position size limits"""
        return {
            "individual_security": 0.05,  # 5% maximum
            "sector_concentration": 0.25,  # 25% maximum
            "geographic_concentration": 0.60,  # 60% maximum in any single country
            "currency_exposure": 0.30  # 30% maximum unhedged foreign currency
        }

    def _identify_prohibited_investments(self, client_profile: InvestorProfile) -> List[str]:
        """Identify prohibited investments"""
        prohibited = ["Penny stocks", "Highly leveraged ETFs (>2x)"]

        # Add based on unique circumstances
        for circumstance in client_profile.unique_circumstances:
            if "no tobacco" in circumstance.lower():
                prohibited.append("Tobacco companies")
            if "no weapons" in circumstance.lower():
                prohibited.append("Defense/weapons manufacturers")

        return prohibited

    def _define_derivative_usage_policy(self, client_profile: InvestorProfile) -> Dict:
        """Define derivative usage policy"""
        if client_profile.risk_tolerance == "conservative":
            return {
                "permitted_derivatives": ["Currency hedging forwards"],
                "prohibited_derivatives": ["Options", "Futures", "Swaps"],
                "usage_purpose": "Hedging only"
            }
        else:
            return {
                "permitted_derivatives": ["Options", "Futures", "Currency hedging"],
                "usage_purpose": "Hedging and limited tactical positioning",
                "maximum_notional": "10% of portfolio value"
            }

    def _select_primary_benchmark(self, strategic_allocation: Dict) -> str:
        """Select primary benchmark based on allocation"""
        allocation = strategic_allocation["strategic_allocation"]

        equity_weight = sum(weight for asset, weight in allocation.items() if "equity" in asset)

        if equity_weight > 0.7:
            return "MSCI All Country World Index"
        elif equity_weight > 0.4:
            return "60/40 Stock/Bond Composite"
        else:
            return "Bloomberg Aggregate Bond Index"

    def _select_secondary_benchmarks(self, strategic_allocation: Dict) -> List[str]:
        """Select secondary benchmarks"""
        allocation = strategic_allocation["strategic_allocation"]
        benchmarks = []

        if "domestic_equity" in allocation:
            benchmarks.append("S&P 500 Index")
        if "international_equity" in allocation:
            benchmarks.append("MSCI EAFE Index")
        if "domestic_bonds" in allocation:
            benchmarks.append("Bloomberg US Aggregate Bond Index")
        if "real_estate" in allocation:
            benchmarks.append("FTSE NAREIT All REITs Index")

        return benchmarks

    def _generate_stress_test_recommendations(self, stress_results: Dict) -> List[str]:
        """Generate recommendations based on stress test results"""
        recommendations = []

        high_impact_scenarios = [scenario for scenario, result in stress_results.items()
                               if result["severity"] == "High"]

        if high_impact_scenarios:
            recommendations.append("Consider reducing portfolio risk through increased diversification")

        if "market_crash" in high_impact_scenarios:
            recommendations.append("Consider adding defensive assets or hedge fund strategies")

        if "inflation_spike" in high_impact_scenarios:
            recommendations.append("Consider adding inflation-protected securities or commodities")

        return recommendations