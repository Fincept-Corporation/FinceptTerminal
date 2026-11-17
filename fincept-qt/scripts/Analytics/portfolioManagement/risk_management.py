
"""Portfolio risk analysis: VaR / ES (historical, normal, Student-t, Cornish-Fisher, Monte Carlo), drawdown,
Euler risk decomposition and instantaneous scenario shocks.

CLI (one JSON object on stdout; {"error": ...} and exit 1 on failure):
  python risk_management.py comprehensive_risk_analysis <returns_data> [weights] [portfolio_value] [beta]
  python risk_management.py comprehensive_risk_analysis '{"returns_data": ..., "weights": ..., "portfolio_value": ..., "beta": ...}'
    returns_data  list of daily returns (fractions) for one portfolio, or {asset: [returns]} for several
    weights       per-asset weights (normalised to sum 1; default equal), portfolio_value default 1,000,000
    beta          portfolio beta to the shocked market for the stress scenarios (default 1, stated in notes)
VaR / ES are positive 1-day loss fractions; confidence 0.95 = the worst 5 % of days.
"""

from datetime import datetime
from typing import Dict, List, Optional, Union, Tuple, Callable
from dataclasses import dataclass

import numpy as np
from scipy import stats

from config import (
    RiskParameters, MathConstants, RiskMetric,
    DEFAULT_RISK_PARAMS, validate_returns, ERROR_MESSAGES,
    CliError, return_series, run_cli
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
            "creation_date": datetime.now().isoformat(timespec="seconds"),
            "last_review": datetime.now().isoformat(timespec="seconds"),
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
        total_assets = financial_metrics.get("total_assets", 1)
        if not total_assets or total_assets <= 0:
            raise ValueError("total_assets must be a positive number")
        liquidity_ratio = financial_metrics.get("liquid_assets", 0) / total_assets
        debt_ratio = financial_metrics.get("total_debt", 0) / total_assets
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
    """Comprehensive Value at Risk calculations (positive VaR = loss fraction)"""

    @staticmethod
    def parametric_var(returns: np.ndarray, confidence_level: float = 0.95,
                       holding_period: int = 1, distribution: str = "normal") -> Dict:
        """Parametric VaR, h-period: mean scaled by h, the dispersion term by sqrt(h).

        normal:          -(mu h + z sigma sqrt(h)),          z = Phi^-1(1 - c)
        t_distribution:  -(loc h + t_df scale sqrt(h)),       Student-t fitted by maximum likelihood
        cornish_fisher:  -(mu h + z_cf sigma sqrt(h)),        z_cf = z + (z^2-1)S/6 + (z^3-3z)K/24
                                                                    - (2z^3-5z)S^2/36
                         (S = skewness, K = EXCESS kurtosis; the S^2 term was missing)
        A series with zero variance has no shape: skewness / kurtosis are None and VaR = -mu h.
        """
        if not validate_returns(returns):
            raise ValueError(ERROR_MESSAGES["insufficient_data"])
        if not 0.0 < confidence_level < 1.0:
            raise ValueError(ERROR_MESSAGES["invalid_confidence_level"])

        returns_array = np.array(returns, dtype=float)
        mean_return = float(np.mean(returns_array))
        std_return = float(np.std(returns_array, ddof=1))
        degenerate = std_return == 0.0
        skewness = None if degenerate or len(returns_array) < 3 else float(stats.skew(returns_array))
        kurtosis = None if degenerate or len(returns_array) < 4 else float(stats.kurtosis(returns_array, fisher=False))
        h = holding_period
        z = stats.norm.ppf(1 - confidence_level)
        extra = {}

        if degenerate:
            var = -mean_return * h
            extra["note"] = "returns have zero variance: VaR is just the (negated) mean"
        elif distribution == "normal":
            var = -(mean_return * h + z * std_return * np.sqrt(h))

        elif distribution == "t_distribution":
            df, loc, scale = stats.t.fit(returns_array)
            t_score = stats.t.ppf(1 - confidence_level, df)
            var = -(loc * h + t_score * scale * np.sqrt(h))
            extra = {"t_degrees_of_freedom": float(df), "t_loc": float(loc), "t_scale": float(scale)}

        elif distribution == "cornish_fisher":
            s = skewness if skewness is not None else 0.0
            k = (kurtosis - 3.0) if kurtosis is not None else 0.0
            z_cf = (z + (z ** 2 - 1) * s / 6 + (z ** 3 - 3 * z) * k / 24
                    - (2 * z ** 3 - 5 * z) * s ** 2 / 36)
            var = -(mean_return * h + z_cf * std_return * np.sqrt(h))
            extra = {"z_normal": float(z), "z_cornish_fisher": float(z_cf)}

        else:
            raise ValueError("Distribution must be 'normal', 't_distribution', or 'cornish_fisher'")

        return {
            "var": float(var),
            "confidence_level": confidence_level,
            "holding_period": holding_period,
            "distribution": distribution,
            "mean_return": mean_return,
            "volatility": std_return,
            "skewness": skewness,
            "kurtosis": kurtosis,
            **extra
        }

    @staticmethod
    def monte_carlo_var(returns: np.ndarray, confidence_level: float = 0.95,
                        holding_period: int = 1, num_simulations: int = 10000,
                        distribution_params: Optional[Dict] = None, seed: int = 42) -> Dict:
        """Monte Carlo VaR from a normal fitted to the returns (h > 1: compounded daily draws).

        Uses its own seeded generator (the global numpy RNG used to be re-seeded on every call).
        The simulated paths themselves are not returned (they were 10,000 numbers per level).
        """
        returns_array = np.array(returns, dtype=float)

        if distribution_params is None:
            distribution_params = {"mean": float(np.mean(returns_array)),
                                   "std": float(np.std(returns_array, ddof=1))}

        rng = np.random.default_rng(seed)
        simulated_returns = rng.normal(
            distribution_params["mean"],
            distribution_params["std"],
            (num_simulations, holding_period)
        )

        if holding_period == 1:
            scenario_returns = simulated_returns[:, 0]
        else:
            scenario_returns = np.prod(1 + simulated_returns, axis=1) - 1

        var = -np.percentile(scenario_returns, (1 - confidence_level) * 100)
        tail = scenario_returns[scenario_returns <= -var]

        return {
            "var": float(var),
            "cvar": float(-np.mean(tail)) if tail.size else float(var),
            "confidence_level": confidence_level,
            "holding_period": holding_period,
            "num_simulations": num_simulations,
            "seed": seed,
            "mean_simulated": float(np.mean(scenario_returns)),
            "std_simulated": float(np.std(scenario_returns, ddof=1))
        }

    @staticmethod
    def component_var(portfolio_returns: np.ndarray, asset_returns: np.ndarray,
                      weights: np.ndarray, confidence_level: float = 0.95) -> Dict:
        """Euler decomposition of parametric (normal) 1-day VaR across assets.

        VaR(w) = -(w.mu + z sqrt(w' S w)) is homogeneous of degree 1 in w, so
            marginal_i  = dVaR/dw_i = -(mu_i + z (S w)_i / sigma_p)
            component_i = w_i x marginal_i,   sum_i component_i = VaR(w)  (exactly)
        S and mu are the sample covariance (ddof=1) and mean of the asset returns, z = Phi^-1(1 - c).
        (The old version bumped one weight, RE-NORMALISED all weights and differenced historical
        VaR, so the components always summed to ~0.)
        diversification_benefit = sum of each position's stand-alone VaR - portfolio VaR (>= 0).
        """
        asset_returns = np.asarray(asset_returns, dtype=float)
        if asset_returns.ndim == 1:
            asset_returns = asset_returns.reshape(-1, 1)
        w = np.asarray(weights, dtype=float)
        mu = asset_returns.mean(axis=0)
        cov = np.atleast_2d(np.cov(asset_returns, rowvar=False, ddof=1))
        z = stats.norm.ppf(1 - confidence_level)
        sigma_p = float(np.sqrt(max(w @ cov @ w, 0.0)))
        historical_var = float(RiskCalculations.value_at_risk_historical(portfolio_returns, confidence_level))

        if sigma_p == 0.0:
            return {
                "method": "parametric normal VaR, Euler allocation",
                "confidence_level": confidence_level,
                "portfolio_var": float(-(w @ mu)),
                "marginal_vars": None, "component_vars": None, "component_percentages": None,
                "note": "portfolio returns have zero variance: no risk to allocate",
                "historical_portfolio_var": historical_var,
            }

        marginal = -(mu + z * (cov @ w) / sigma_p)
        component = w * marginal
        portfolio_var = float(-(w @ mu + z * sigma_p))
        standalone = w * -(mu + z * np.sqrt(np.clip(np.diag(cov), 0.0, None)))

        return {
            "method": "parametric normal VaR, Euler allocation (components sum to portfolio_var)",
            "confidence_level": confidence_level,
            "portfolio_var": portfolio_var,
            "marginal_vars": marginal,
            "component_vars": component,
            "component_percentages": (component / portfolio_var * 100.0) if portfolio_var != 0 else None,
            "components_sum": float(component.sum()),
            "standalone_vars": standalone,
            "diversification_benefit": float(standalone.sum() - portfolio_var),
            "historical_portfolio_var": historical_var,
        }


def _drawdown_path(returns: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """(wealth incl. the starting 1.0, drawdown from the running peak) - so a loss on the FIRST
    observation counts (cumprod without the starting 1.0 made it the peak)."""
    wealth = np.concatenate(([1.0], np.cumprod(1.0 + np.asarray(returns, dtype=float))))
    return wealth, wealth / np.maximum.accumulate(wealth) - 1.0


class ScenarioAnalysis:
    """Scenario-based risk analysis"""

    @staticmethod
    def stress_testing(returns: np.ndarray, stress_scenarios: Dict[str, float],
                       beta: Optional[float] = None, portfolio_value: Optional[float] = None) -> Dict:
        """Instantaneous scenario shocks, each applied ONCE to today's portfolio.

        A scenario is a one-off market move (e.g. -20 %). The portfolio moves shock x beta (beta 1
        when no beta is given). Reported: that return and its money loss, the drawdown from the
        running peak if it hit today, and the maximum drawdown of the history with the shock
        appended. (The old version added the shock to EVERY daily return and reported a -20 %
        per-day "stressed mean".)
        """
        r = np.asarray(returns, dtype=float)
        b = 1.0 if beta is None or not np.isfinite(beta) else float(beta)
        wealth, _ = _drawdown_path(r)
        peak, now = float(wealth.max()), float(wealth[-1])
        var_99 = float(RiskCalculations.value_at_risk_historical(r, 0.99)) if r.size else float("nan")
        results = {}

        for scenario_name, shock in stress_scenarios.items():
            port_ret = float(shock) * b
            _, dd_with = _drawdown_path(np.append(r, port_ret))
            results[scenario_name] = {
                "shock_applied": float(shock),
                "assumed_beta": b,
                "portfolio_return": port_ret,
                "loss_fraction": -port_ret,
                "loss_amount": -port_ret * portfolio_value if portfolio_value is not None else None,
                "value_after_shock": portfolio_value * (1.0 + port_ret) if portfolio_value is not None else None,
                "drawdown_from_peak_after_shock": now * (1.0 + port_ret) / peak - 1.0,
                "max_drawdown_including_shock": float(dd_with.min()),
                "multiple_of_1d_var_99": (-port_ret / var_99) if var_99 > 0 else None,
            }

        return results

    @staticmethod
    def historical_scenario_analysis(current_portfolio: np.ndarray,
                                     historical_market_data: Dict[str, np.ndarray],
                                     crisis_periods: Dict[str, Tuple[str, str]]) -> Dict:
        """Not available - always returns {"error": ...}.

        It never worked: the date filter was a placeholder (every crisis used the whole series), it
        called len() on a float and its portfolio figure was a fixed string. A working version needs
        dated return series; the live app already has one (scripts/portfolio_metrics.py
        historical_stress, the Portfolio screen's stress test).
        """
        return {"error": "historical_scenario_analysis is not available: it needs dated return series and "
                         "was never implemented here. Use scripts/portfolio_metrics.py historical_stress "
                         "(the Portfolio screen's stress test) instead."}


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
                                    portfolio_value: float = 1000000,
                                    beta: Optional[float] = None) -> Dict:
        """Perform comprehensive risk analysis.

        returns_data: one return series, or {asset: series} (weights then combine them; equal by
        default). Series of different lengths are aligned on their most recent common observations.
        """
        notes: List[str] = []

        if isinstance(returns_data, dict):
            if not returns_data:
                raise ValueError("returns_data is empty")
            asset_names = list(returns_data.keys())
            series = [np.asarray(returns_data[a], dtype=float) for a in asset_names]
            n_common = min(len(s) for s in series)
            if any(len(s) != n_common for s in series):
                notes.append(f"series had different lengths; aligned on the most recent {n_common} "
                             "observations (assumes they all end on the same date)")
            returns_matrix = np.column_stack([s[len(s) - n_common:] for s in series])
            if weights is None:
                weights = np.ones(len(asset_names)) / len(asset_names)
            weights = np.asarray(weights, dtype=float).reshape(-1)
            if weights.size != len(asset_names):
                raise ValueError(f"weights has {weights.size} entries for {len(asset_names)} assets")
            if not np.all(np.isfinite(weights)) or weights.sum() <= 0:
                raise ValueError("weights must be finite and sum to a positive number")
            if abs(weights.sum() - 1.0) > 1e-6:
                notes.append(f"weights summed to {weights.sum():g}; normalised to 1")
                weights = weights / weights.sum()
            portfolio_returns = returns_matrix @ weights
        else:
            portfolio_returns = np.asarray(returns_data, dtype=float)
            returns_matrix = portfolio_returns.reshape(-1, 1)
            weights = np.array([1.0])
            asset_names = ["Portfolio"]

        n = len(portfolio_returns)
        if n < 2:
            raise ValueError(f"At least 2 returns are required (got {n})")
        if n < 30:
            notes.append(f"only {n} observations: VaR / ES and the fitted distributions are very uncertain")
        if np.std(portfolio_returns, ddof=1) == 0:
            notes.append("portfolio returns have zero variance: skewness, kurtosis and the normality test "
                         "are undefined (null)")
        if beta is None:
            notes.append("stress scenarios assume the portfolio moves one-for-one with the shocked market "
                         "(beta 1); pass beta to change that")

        results = {
            "observations": n,
            "basic_risk_metrics": self._calculate_basic_risk_metrics(portfolio_returns),
            "var_analysis": self._comprehensive_var_analysis(portfolio_returns),
            "stress_testing": ScenarioAnalysis.stress_testing(
                portfolio_returns, self.parameters.stress_scenarios, beta, portfolio_value),
            "risk_decomposition": None
        }

        if len(asset_names) > 1:
            decomposition = VaRCalculations.component_var(portfolio_returns, returns_matrix, weights)
            decomposition["assets"] = asset_names
            decomposition["weights"] = weights
            results["risk_decomposition"] = decomposition

        results["dollar_metrics"] = self._convert_to_dollar_metrics(results, portfolio_value)
        results["portfolio_value"] = portfolio_value
        results["conventions"] = ("VaR / ES: positive 1-day loss fractions of portfolio value; downside "
                                  "deviation: target semideviation below 0 over all observations x sqrt(252)")
        results["notes"] = notes

        return results

    def _calculate_basic_risk_metrics(self, returns: np.ndarray) -> Dict:
        """Calculate basic risk metrics"""
        n = len(returns)
        sd = float(np.std(returns, ddof=1))
        shaped = sd > 0
        jb = stats.jarque_bera(returns) if shaped and n >= 3 else None
        return {
            "volatility_daily": sd,
            "volatility_annual": sd * np.sqrt(MathConstants.TRADING_DAYS_YEAR),
            "downside_deviation": StatisticalCalculations.calculate_downside_deviation(returns),
            "downside_deviation_daily": StatisticalCalculations.calculate_downside_deviation(returns, annualize=False),
            "max_drawdown": self._calculate_max_drawdown(returns),
            "skewness": float(stats.skew(returns)) if shaped and n >= 3 else None,
            "kurtosis": float(stats.kurtosis(returns, fisher=False)) if shaped and n >= 4 else None,
            "jarque_bera_test": {"statistic": float(jb[0]), "p_value": float(jb[1])} if jb is not None else None
        }

    def _comprehensive_var_analysis(self, returns: np.ndarray) -> Dict:
        """Comprehensive VaR analysis with multiple methods (1-day)"""
        var_results = {}
        degenerate = float(np.std(returns, ddof=1)) == 0.0

        for confidence_level in self.parameters.var_confidence_levels:
            var_results[f"var_{int(round(confidence_level * 100))}"] = {
                "parametric_normal": VaRCalculations.parametric_var(
                    returns, confidence_level, distribution="normal"
                ),
                "parametric_t": None if degenerate else VaRCalculations.parametric_var(
                    returns, confidence_level, distribution="t_distribution"
                ),
                "parametric_cornish_fisher": VaRCalculations.parametric_var(
                    returns, confidence_level, distribution="cornish_fisher"
                ),
                "historical": {
                    "var": RiskCalculations.value_at_risk_historical(returns, confidence_level),
                    "cvar": RiskCalculations.conditional_value_at_risk(returns, confidence_level)
                },
                "parametric_normal_cvar": RiskCalculations.conditional_value_at_risk(
                    returns, confidence_level, method="parametric"
                ),
                "monte_carlo": VaRCalculations.monte_carlo_var(
                    returns, confidence_level, num_simulations=self.parameters.monte_carlo_simulations
                )
            }

        return var_results

    def _perform_stress_testing(self, returns: np.ndarray) -> Dict:
        """Perform comprehensive stress testing"""
        return ScenarioAnalysis.stress_testing(returns, self.parameters.stress_scenarios)

    def _calculate_max_drawdown(self, returns: np.ndarray) -> Dict:
        """Maximum drawdown from the running peak of wealth (starting wealth 1.0 included).

        Indices count observations: 0 = before the first return, k = after the k-th return.
        """
        wealth, drawdown = _drawdown_path(returns)

        trough_idx = int(np.argmin(drawdown))
        peak_idx = int(np.argmax(wealth[:trough_idx + 1]))
        recovered = np.nonzero(wealth[trough_idx:] >= wealth[peak_idx])[0]
        recovery_idx = int(trough_idx + recovered[0]) if trough_idx > peak_idx and recovered.size else None

        return {
            "max_drawdown": float(drawdown.min()),
            "peak_index": peak_idx,
            "trough_index": trough_idx,
            "peak_to_trough_days": trough_idx - peak_idx,
            "recovery_index": recovery_idx,
            "current_drawdown": float(drawdown[-1]),
            "drawdown_series": drawdown[1:]
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

        decomposition = results.get("risk_decomposition")
        if decomposition and decomposition.get("component_vars") is not None:
            dollar_metrics["component_var_95"] = dict(zip(
                decomposition["assets"], np.asarray(decomposition["component_vars"]) * portfolio_value))

        return dollar_metrics

# ============================================================================
# CLI Interface
# ============================================================================

_ARGMAP = {"comprehensive_risk_analysis": ["returns_data", "weights", "portfolio_value", "beta"]}


def _cmd_comprehensive_risk_analysis(args) -> Dict:
    returns_data = args.get_json(0, "returns_data")
    weights = args.get_json(1, "weights")
    portfolio_value = args.get_float(2, "portfolio_value", 1000000.0)
    beta = args.get_float(3, "beta")
    if returns_data is None:
        raise CliError("returns_data is required: a list of daily returns or {asset: [returns]}")
    if isinstance(returns_data, dict):
        if not returns_data:
            raise CliError("returns_data is empty")
        data = {str(k): return_series(v, f"returns_data[{k}]") for k, v in returns_data.items()}
    else:
        data = return_series(returns_data, "returns_data")
    if weights is not None:
        weights = return_series(weights, "weights", min_len=1)
    if portfolio_value <= 0:
        raise CliError("portfolio_value must be positive")
    return RiskManagement().comprehensive_risk_analysis(data, weights, portfolio_value, beta)


if __name__ == "__main__":
    run_cli({"comprehensive_risk_analysis": _cmd_comprehensive_risk_analysis}, _ARGMAP)
