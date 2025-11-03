
"""Portfolio Risk Management Module
===============================

Portfolio risk management and mitigation

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
from typing import Dict, List, Optional, Union, Tuple, Callable
from dataclasses import dataclass
from scipy import stats
import warnings

from config import (
    RiskParameters, MathConstants, RiskMetric,
    DEFAULT_RISK_PARAMS, validate_returns, ERROR_MESSAGES
)
from math_engine import RiskCalculations, StatisticalCalculations


@dataclass
class RiskBudget:
    """Risk budget allocation structure"""
    portfolio_id: str
    total_risk_budget: float
    asset_allocations: Dict[str, float]
    risk_allocations: Dict[str, float]
    risk_limits: Dict[str, float]


class RiskGovernance:
    """Risk governance framework implementation"""

    def __init__(self):
        self.risk_policies = {}
        self.risk_limits = {}
        self.governance_structure = {}

    def define_risk_policy(self, policy_name: str, policy_details: Dict) -> None:
        """Define risk management policy"""
        required_fields = ["objective", "scope", "methodology", "limits", "reporting"]

        for field in required_fields:
            if field not in policy_details:
                raise ValueError(f"Risk policy must include '{field}'")

        self.risk_policies[policy_name] = {
            "policy_details": policy_details,
            "creation_date": pd.Timestamp.now(),
            "last_review": pd.Timestamp.now(),
            "status": "active"
        }

    def set_risk_limits(self, limit_type: str, limits: Dict) -> None:
        """Set various types of risk limits"""
        valid_types = ["position", "sector", "var", "tracking_error", "leverage", "concentration"]

        if limit_type not in valid_types:
            raise ValueError(f"Limit type must be one of {valid_types}")

        self.risk_limits[limit_type] = limits

    def assess_governance_effectiveness(self) -> Dict:
        """Assess risk governance effectiveness"""
        return {
            "policies_defined": len(self.risk_policies),
            "limits_established": len(self.risk_limits),
            "governance_score": self._calculate_governance_score(),
            "areas_for_improvement": self._identify_improvement_areas()
        }

    def _calculate_governance_score(self) -> float:
        """Calculate governance effectiveness score"""
        base_score = 0

        # Policy coverage
        if len(self.risk_policies) >= 3:
            base_score += 30
        elif len(self.risk_policies) >= 1:
            base_score += 15

        # Limit coverage
        if len(self.risk_limits) >= 4:
            base_score += 40
        elif len(self.risk_limits) >= 2:
            base_score += 20

        # Structure completeness
        required_structures = ["risk_committee", "cro", "risk_reporting"]
        structure_score = sum(20 for struct in required_structures
                              if struct in self.governance_structure)
        base_score += structure_score

        return min(100, base_score)

    def _identify_improvement_areas(self) -> List[str]:
        """Identify areas needing improvement"""
        improvements = []

        if len(self.risk_policies) < 3:
            improvements.append("Expand risk policy coverage")

        if "var" not in self.risk_limits:
            improvements.append("Establish VaR limits")

        if "concentration" not in self.risk_limits:
            improvements.append("Set concentration limits")

        required_structures = ["risk_committee", "cro", "risk_reporting"]
        missing_structures = [s for s in required_structures
                              if s not in self.governance_structure]
        if missing_structures:
            improvements.append(f"Establish: {', '.join(missing_structures)}")

        return improvements


class RiskTolerance:
    """Risk tolerance analysis and measurement"""

    @staticmethod
    def assess_risk_capacity(financial_metrics: Dict) -> Dict:
        """Assess quantitative risk capacity"""
        # Financial capacity indicators
        liquidity_ratio = financial_metrics.get("liquid_assets", 0) / financial_metrics.get("total_assets", 1)
        debt_ratio = financial_metrics.get("total_debt", 0) / financial_metrics.get("total_assets", 1)
        income_stability = financial_metrics.get("income_stability_score", 5)  # 1-10 scale
        time_horizon = financial_metrics.get("investment_horizon_years", 10)

        # Calculate capacity score
        capacity_score = 0

        # Liquidity component (25%)
        if liquidity_ratio > 0.5:
            capacity_score += 25
        elif liquidity_ratio > 0.3:
            capacity_score += 15
        elif liquidity_ratio > 0.1:
            capacity_score += 10

        # Debt component (25%)
        if debt_ratio < 0.2:
            capacity_score += 25
        elif debt_ratio < 0.4:
            capacity_score += 15
        elif debt_ratio < 0.6:
            capacity_score += 10

        # Income stability (25%)
        capacity_score += (income_stability / 10) * 25

        # Time horizon (25%)
        if time_horizon > 20:
            capacity_score += 25
        elif time_horizon > 10:
            capacity_score += 20
        elif time_horizon > 5:
            capacity_score += 15
        elif time_horizon > 2:
            capacity_score += 10

        return {
            "capacity_score": capacity_score,
            "capacity_level": "High" if capacity_score > 75 else
            "Moderate" if capacity_score > 50 else "Low",
            "liquidity_ratio": liquidity_ratio,
            "debt_ratio": debt_ratio,
            "limiting_factors": RiskTolerance._identify_limiting_factors(
                liquidity_ratio, debt_ratio, income_stability, time_horizon
            )
        }

    @staticmethod
    def assess_risk_willingness(questionnaire_responses: Dict) -> Dict:
        """Assess behavioral risk willingness"""
        # Sample risk tolerance questionnaire scoring
        willingness_score = 0

        # Loss tolerance (30%)
        loss_comfort = questionnaire_responses.get("max_acceptable_loss", 5)  # 1-10 scale
        willingness_score += (loss_comfort / 10) * 30

        # Volatility comfort (25%)
        volatility_comfort = questionnaire_responses.get("volatility_comfort", 5)
        willingness_score += (volatility_comfort / 10) * 25

        # Investment experience (20%)
        experience_level = questionnaire_responses.get("investment_experience", 5)
        willingness_score += (experience_level / 10) * 20

        # Risk understanding (25%)
        risk_knowledge = questionnaire_responses.get("risk_knowledge", 5)
        willingness_score += (risk_knowledge / 10) * 25

        return {
            "willingness_score": willingness_score,
            "willingness_level": "High" if willingness_score > 75 else
            "Moderate" if willingness_score > 50 else "Low",
            "emotional_factors": RiskTolerance._analyze_emotional_factors(questionnaire_responses)
        }

    @staticmethod
    def reconcile_capacity_willingness(capacity_result: Dict, willingness_result: Dict) -> Dict:
        """Reconcile capacity and willingness to determine overall risk tolerance"""
        capacity_score = capacity_result["capacity_score"]
        willingness_score = willingness_result["willingness_score"]

        # Use the lower of the two scores (conservative approach)
        overall_score = min(capacity_score, willingness_score)

        # Identify mismatches
        mismatch = abs(capacity_score - willingness_score)
        significant_mismatch = mismatch > 30

        return {
            "overall_risk_tolerance": overall_score,
            "risk_level": "High" if overall_score > 75 else
            "Moderate" if overall_score > 50 else "Low",
            "capacity_willingness_mismatch": significant_mismatch,
            "mismatch_magnitude": mismatch,
            "recommended_approach": RiskTolerance._recommend_approach(
                capacity_score, willingness_score, significant_mismatch
            )
        }

    @staticmethod
    def _identify_limiting_factors(liquidity_ratio: float, debt_ratio: float,
                                   income_stability: float, time_horizon: float) -> List[str]:
        """Identify factors limiting risk capacity"""
        factors = []

        if liquidity_ratio < 0.2:
            factors.append("Low liquidity")
        if debt_ratio > 0.5:
            factors.append("High debt burden")
        if income_stability < 5:
            factors.append("Income instability")
        if time_horizon < 5:
            factors.append("Short time horizon")

        return factors

    @staticmethod
    def _analyze_emotional_factors(responses: Dict) -> List[str]:
        """Analyze emotional factors affecting willingness"""
        factors = []

        if responses.get("loss_aversion", 5) > 7:
            factors.append("High loss aversion")
        if responses.get("market_timing_belief", 5) > 7:
            factors.append("Market timing bias")
        if responses.get("overconfidence", 5) > 7:
            factors.append("Overconfidence bias")

        return factors

    @staticmethod
    def _recommend_approach(capacity: float, willingness: float, mismatch: bool) -> str:
        """Recommend approach based on capacity/willingness analysis"""
        if not mismatch:
            return "Proceed with aligned risk tolerance level"
        elif capacity > willingness:
            return "Education and gradual risk increase - capacity exceeds willingness"
        else:
            return "Conservative approach recommended - willingness exceeds capacity"


class VaRCalculations:
    """Comprehensive Value at Risk calculations"""

    @staticmethod
    def parametric_var(returns: np.ndarray, confidence_level: float = 0.95,
                       holding_period: int = 1, distribution: str = "normal") -> Dict:
        """Calculate parametric VaR with multiple distributions"""
        if not validate_returns(returns):
            raise ValueError(ERROR_MESSAGES["insufficient_data"])

        returns_array = np.array(returns)

        if distribution == "normal":
            mean_return = np.mean(returns_array)
            std_return = np.std(returns_array, ddof=1)
            z_score = stats.norm.ppf(1 - confidence_level)
            var = -(mean_return + z_score * std_return) * np.sqrt(holding_period)

        elif distribution == "t_distribution":
            # Fit t-distribution
            params = stats.t.fit(returns_array)
            df, loc, scale = params
            t_score = stats.t.ppf(1 - confidence_level, df)
            var = -(loc + t_score * scale) * np.sqrt(holding_period)

        elif distribution == "cornish_fisher":
            # Cornish-Fisher expansion for skewness and kurtosis
            mean_return = np.mean(returns_array)
            std_return = np.std(returns_array, ddof=1)
            skewness = stats.skew(returns_array)
            kurtosis = stats.kurtosis(returns_array, fisher=False)

            z = stats.norm.ppf(1 - confidence_level)
            z_cf = z + (z ** 2 - 1) * skewness / 6 + (z ** 3 - 3 * z) * (kurtosis - 3) / 24
            var = -(mean_return + z_cf * std_return) * np.sqrt(holding_period)

        else:
            raise ValueError("Distribution must be 'normal', 't_distribution', or 'cornish_fisher'")

        return {
            "var": var,
            "confidence_level": confidence_level,
            "holding_period": holding_period,
            "distribution": distribution,
            "mean_return": np.mean(returns_array),
            "volatility": np.std(returns_array, ddof=1),
            "skewness": stats.skew(returns_array),
            "kurtosis": stats.kurtosis(returns_array, fisher=False)
        }

    @staticmethod
    def monte_carlo_var(returns: np.ndarray, confidence_level: float = 0.95,
                        holding_period: int = 1, num_simulations: int = 10000,
                        distribution_params: Optional[Dict] = None) -> Dict:
        """Calculate Monte Carlo VaR"""
        returns_array = np.array(returns)

        # Fit distribution parameters if not provided
        if distribution_params is None:
            mean_return = np.mean(returns_array)
            std_return = np.std(returns_array, ddof=1)
            distribution_params = {"mean": mean_return, "std": std_return}

        # Generate random scenarios
        np.random.seed(42)  # For reproducibility
        simulated_returns = np.random.normal(
            distribution_params["mean"],
            distribution_params["std"],
            (num_simulations, holding_period)
        )

        # Calculate portfolio returns for each simulation
        if holding_period == 1:
            scenario_returns = simulated_returns.flatten()
        else:
            # Compound returns over holding period
            scenario_returns = np.prod(1 + simulated_returns, axis=1) - 1

        # Calculate VaR
        var = -np.percentile(scenario_returns, (1 - confidence_level) * 100)

        return {
            "var": var,
            "confidence_level": confidence_level,
            "holding_period": holding_period,
            "num_simulations": num_simulations,
            "simulated_returns": scenario_returns,
            "mean_simulated": np.mean(scenario_returns),
            "std_simulated": np.std(scenario_returns)
        }

    @staticmethod
    def component_var(portfolio_returns: np.ndarray, asset_returns: np.ndarray,
                      weights: np.ndarray, confidence_level: float = 0.95) -> Dict:
        """Calculate component VaR contributions"""
        # Portfolio VaR
        portfolio_var = RiskCalculations.value_at_risk_historical(portfolio_returns, confidence_level)

        # Calculate marginal VaR for each asset
        marginal_vars = []
        component_vars = []

        for i in range(len(weights)):
            # Small perturbation to weight
            epsilon = 0.001
            perturbed_weights = weights.copy()
            perturbed_weights[i] += epsilon
            perturbed_weights = perturbed_weights / np.sum(perturbed_weights)  # Renormalize

            # Calculate perturbed portfolio returns
            perturbed_portfolio_returns = np.dot(asset_returns, perturbed_weights)
            perturbed_var = RiskCalculations.value_at_risk_historical(perturbed_portfolio_returns, confidence_level)

            # Marginal VaR
            marginal_var = (perturbed_var - portfolio_var) / epsilon
            marginal_vars.append(marginal_var)

            # Component VaR
            component_var = marginal_var * weights[i]
            component_vars.append(component_var)

        return {
            "portfolio_var": portfolio_var,
            "marginal_vars": np.array(marginal_vars),
            "component_vars": np.array(component_vars),
            "component_percentages": np.array(component_vars) / np.sum(np.abs(component_vars)) * 100,
            "diversification_benefit": portfolio_var - np.sum(component_vars)
        }


class ScenarioAnalysis:
    """Scenario-based risk analysis"""

    @staticmethod
    def stress_testing(returns: np.ndarray, stress_scenarios: Dict[str, float]) -> Dict:
        """Perform stress testing on portfolio"""
        results = {}

        for scenario_name, shock in stress_scenarios.items():
            # Apply shock to returns
            stressed_returns = returns + shock

            # Calculate metrics under stress
            stressed_mean = np.mean(stressed_returns)
            stressed_std = np.std(stressed_returns, ddof=1)
            stressed_var_95 = RiskCalculations.value_at_risk_historical(stressed_returns, 0.95)
            stressed_var_99 = RiskCalculations.value_at_risk_historical(stressed_returns, 0.99)

            results[scenario_name] = {
                "shock_applied": shock,
                "stressed_mean_return": stressed_mean,
                "stressed_volatility": stressed_std,
                "stressed_var_95": stressed_var_95,
                "stressed_var_99": stressed_var_99,
                "return_impact": stressed_mean - np.mean(returns),
                "volatility_impact": stressed_std - np.std(returns, ddof=1)
            }

        return results

    @staticmethod
    def historical_scenario_analysis(current_portfolio: np.ndarray,
                                     historical_market_data: Dict[str, np.ndarray],
                                     crisis_periods: Dict[str, Tuple[str, str]]) -> Dict:
        """Analyze portfolio performance during historical crisis periods"""
        results = {}

        for crisis_name, (start_date, end_date) in crisis_periods.items():
            crisis_results = {}

            for asset, returns in historical_market_data.items():
                # Filter returns for crisis period (simplified - assumes daily data)
                # In practice, would need proper date filtering
                crisis_returns = returns  # Placeholder

                if len(crisis_returns) > 0:
                    crisis_results[asset] = {
                        "total_return": np.prod(1 + crisis_returns) - 1,
                        "volatility": np.std(crisis_returns, ddof=1) * np.sqrt(252),
                        "max_drawdown": np.min(np.cumsum(crisis_returns)),
                        "worst_day": np.min(crisis_returns)
                    }

            # Portfolio-level crisis analysis
            if crisis_results:
                portfolio_crisis_returns = np.zeros(len(next(iter(crisis_results.values()))["total_return"]))
                # This would need proper implementation based on actual data structure

                results[crisis_name] = {
                    "period": f"{start_date} to {end_date}",
                    "asset_performance": crisis_results,
                    "portfolio_impact": "Calculation needed with proper data structure"
                }

        return results


class RiskBudgetingSystem:
    """Risk budgeting and allocation framework"""

    def __init__(self, total_risk_budget: float):
        self.total_risk_budget = total_risk_budget
        self.allocations = {}
        self.current_utilization = 0.0

    def allocate_risk_budget(self, allocation_name: str, risk_amount: float,
                             allocation_type: str = "absolute") -> None:
        """Allocate portion of risk budget"""
        if allocation_type == "absolute":
            if self.current_utilization + risk_amount > self.total_risk_budget:
                raise ValueError("Risk allocation exceeds total budget")

            self.allocations[allocation_name] = {
                "risk_amount": risk_amount,
                "percentage": risk_amount / self.total_risk_budget * 100,
                "type": "absolute"
            }
            self.current_utilization += risk_amount

        elif allocation_type == "percentage":
            risk_amount_abs = risk_amount * self.total_risk_budget / 100
            if self.current_utilization + risk_amount_abs > self.total_risk_budget:
                raise ValueError("Risk allocation exceeds total budget")

            self.allocations[allocation_name] = {
                "risk_amount": risk_amount_abs,
                "percentage": risk_amount,
                "type": "percentage"
            }
            self.current_utilization += risk_amount_abs

    def monitor_risk_utilization(self, current_risks: Dict[str, float]) -> Dict:
        """Monitor current risk utilization vs. budget"""
        utilization_report = {}
        total_current_risk = 0

        for allocation_name, budget_info in self.allocations.items():
            current_risk = current_risks.get(allocation_name, 0)
            budgeted_risk = budget_info["risk_amount"]

            utilization_report[allocation_name] = {
                "budgeted_risk": budgeted_risk,
                "current_risk": current_risk,
                "utilization_percentage": current_risk / budgeted_risk * 100 if budgeted_risk > 0 else 0,
                "excess_risk": max(0, current_risk - budgeted_risk),
                "available_budget": max(0, budgeted_risk - current_risk)
            }

            total_current_risk += current_risk

        return {
            "individual_allocations": utilization_report,
            "total_budget": self.total_risk_budget,
            "total_current_risk": total_current_risk,
            "overall_utilization": total_current_risk / self.total_risk_budget * 100,
            "remaining_budget": self.total_risk_budget - total_current_risk,
            "budget_breaches": [name for name, info in utilization_report.items()
                                if info["excess_risk"] > 0]
        }

    def rebalance_risk_budget(self, target_allocations: Dict[str, float]) -> Dict:
        """Rebalance risk budget allocations"""
        if sum(target_allocations.values()) > 100:
            raise ValueError("Target allocations exceed 100%")

        rebalancing_plan = {}

        for allocation_name, target_percentage in target_allocations.items():
            target_risk = target_percentage * self.total_risk_budget / 100
            current_risk = self.allocations.get(allocation_name, {}).get("risk_amount", 0)

            rebalancing_plan[allocation_name] = {
                "current_allocation": current_risk,
                "target_allocation": target_risk,
                "adjustment_needed": target_risk - current_risk,
                "adjustment_percentage": ((target_risk - current_risk) / current_risk * 100) if current_risk > 0 else 0
            }

        return {
            "rebalancing_plan": rebalancing_plan,
            "total_adjustments": sum(abs(info["adjustment_needed"]) for info in rebalancing_plan.values()),
            "implementation_priority": self._prioritize_adjustments(rebalancing_plan)
        }

    def _prioritize_adjustments(self, rebalancing_plan: Dict) -> List[str]:
        """Prioritize risk budget adjustments"""
        # Prioritize by magnitude of adjustment needed
        adjustments = [(name, abs(info["adjustment_needed"]))
                       for name, info in rebalancing_plan.items()]
        adjustments.sort(key=lambda x: x[1], reverse=True)

        return [name for name, _ in adjustments]


class RiskManagement:
    """Main risk management interface"""

    def __init__(self, parameters: RiskParameters = DEFAULT_RISK_PARAMS):
        self.parameters = parameters
        self.governance = RiskGovernance()
        self.risk_budgeting = None

    def comprehensive_risk_analysis(self, returns_data: Union[np.ndarray, Dict[str, np.ndarray]],
                                    weights: Optional[np.ndarray] = None,
                                    portfolio_value: float = 1000000) -> Dict:
        """Perform comprehensive risk analysis"""

        if isinstance(returns_data, dict):
            # Multiple assets
            returns_matrix = np.array([returns_data[asset] for asset in returns_data.keys()]).T
            if weights is None:
                weights = np.ones(len(returns_data)) / len(returns_data)
            portfolio_returns = np.dot(returns_matrix, weights)
            asset_names = list(returns_data.keys())
        else:
            # Single asset or portfolio
            portfolio_returns = np.array(returns_data)
            returns_matrix = portfolio_returns.reshape(-1, 1)
            weights = np.array([1.0])
            asset_names = ["Portfolio"]

        results = {
            "basic_risk_metrics": self._calculate_basic_risk_metrics(portfolio_returns),
            "var_analysis": self._comprehensive_var_analysis(portfolio_returns),
            "stress_testing": self._perform_stress_testing(portfolio_returns),
            "risk_decomposition": None
        }

        # Component analysis for multi-asset portfolios
        if len(asset_names) > 1:
            results["risk_decomposition"] = VaRCalculations.component_var(
                portfolio_returns, returns_matrix, weights
            )

        # Convert to dollar amounts
        results["dollar_metrics"] = self._convert_to_dollar_metrics(results, portfolio_value)

        return results

    def _calculate_basic_risk_metrics(self, returns: np.ndarray) -> Dict:
        """Calculate basic risk metrics"""
        return {
            "volatility_daily": np.std(returns, ddof=1),
            "volatility_annual": np.std(returns, ddof=1) * np.sqrt(MathConstants.TRADING_DAYS_YEAR),
            "downside_deviation": StatisticalCalculations.calculate_downside_deviation(returns),
            "max_drawdown": self._calculate_max_drawdown(returns),
            "skewness": stats.skew(returns),
            "kurtosis": stats.kurtosis(returns, fisher=False),
            "jarque_bera_test": stats.jarque_bera(returns)
        }

    def _comprehensive_var_analysis(self, returns: np.ndarray) -> Dict:
        """Comprehensive VaR analysis with multiple methods"""
        var_results = {}

        for confidence_level in self.parameters.var_confidence_levels:
            var_results[f"var_{int(confidence_level * 100)}"] = {
                "parametric_normal": VaRCalculations.parametric_var(
                    returns, confidence_level, distribution="normal"
                ),
                "parametric_t": VaRCalculations.parametric_var(
                    returns, confidence_level, distribution="t_distribution"
                ),
                "historical": {
                    "var": RiskCalculations.value_at_risk_historical(returns, confidence_level),
                    "cvar": RiskCalculations.conditional_value_at_risk(returns, confidence_level)
                },
                "monte_carlo": VaRCalculations.monte_carlo_var(
                    returns, confidence_level, num_simulations=self.parameters.monte_carlo_simulations
                )
            }

        return var_results

    def _perform_stress_testing(self, returns: np.ndarray) -> Dict:
        """Perform comprehensive stress testing"""
        return ScenarioAnalysis.stress_testing(returns, self.parameters.stress_scenarios)

    def _calculate_max_drawdown(self, returns: np.ndarray) -> Dict:
        """Calculate maximum drawdown statistics"""
        cumulative_returns = np.cumprod(1 + returns)
        running_max = np.maximum.accumulate(cumulative_returns)
        drawdown = (cumulative_returns - running_max) / running_max

        max_dd = np.min(drawdown)
        max_dd_idx = np.argmin(drawdown)
        peak_idx = np.argmax(running_max[:max_dd_idx + 1]) if max_dd_idx > 0 else 0

        return {
            "max_drawdown": max_dd,
            "peak_to_trough_days": max_dd_idx - peak_idx,
            "drawdown_series": drawdown
        }

    def _convert_to_dollar_metrics(self, results: Dict, portfolio_value: float) -> Dict:
        """Convert percentage metrics to dollar amounts"""
        dollar_metrics = {}

        if "var_analysis" in results:
            for var_level, var_data in results["var_analysis"].items():
                dollar_metrics[var_level] = {}

                if "historical" in var_data:
                    dollar_metrics[var_level]["historical_var"] = var_data["historical"]["var"] * portfolio_value
                    dollar_metrics[var_level]["historical_cvar"] = var_data["historical"]["cvar"] * portfolio_value

                if "parametric_normal" in var_data:
                    dollar_metrics[var_level]["parametric_var"] = var_data["parametric_normal"]["var"] * portfolio_value

        return dollar_metrics