"""
Risk Analysis Tools for Risk Quants

Tools for measuring, monitoring, and managing portfolio risk.
"""

from typing import Dict, Any, List, Optional
from agno.tools import Toolkit


class RiskAnalysisTools(Toolkit):
    """Tools for risk analysis and monitoring"""

    def __init__(self):
        super().__init__(name="risk_analysis_tools")
        self.register(self.calculate_var)
        self.register(self.run_stress_test)
        self.register(self.analyze_factor_exposure)
        self.register(self.calculate_correlation_risk)
        self.register(self.check_concentration)
        self.register(self.monitor_drawdown)
        self.register(self.calculate_tail_risk)
        self.register(self.assess_liquidity_risk)

    def calculate_var(
        self,
        portfolio: Dict[str, float],
        confidence_level: float = 0.99,
        horizon_days: int = 1,
        method: str = "historical"
    ) -> Dict[str, Any]:
        """
        Calculate Value at Risk for portfolio.

        Args:
            portfolio: Dictionary of ticker -> position size
            confidence_level: VaR confidence level (0.95, 0.99)
            horizon_days: Time horizon
            method: VaR method (historical, parametric, monte_carlo)

        Returns:
            VaR metrics
        """
        return {
            "var_pct": 2.1,  # 2.1% daily VaR at 99%
            "var_dollars": 21000000,
            "confidence_level": confidence_level,
            "horizon_days": horizon_days,
            "method": method,
            "expected_shortfall": 3.2,  # CVaR/ES
            "component_var": {
                "equity": 1.5,
                "rates": 0.3,
                "fx": 0.2,
                "commodity": 0.1,
            },
            "marginal_var_top_5": [
                {"ticker": "AAPL", "marginal_var": 0.15},
                {"ticker": "MSFT", "marginal_var": 0.12},
                {"ticker": "GOOGL", "marginal_var": 0.10},
            ],
            "var_limit": 2.0,
            "limit_utilization": 1.05,  # Slightly over limit
            "breach": True,
        }

    def run_stress_test(
        self,
        portfolio: Dict[str, float],
        scenarios: List[str]
    ) -> Dict[str, Any]:
        """
        Run stress tests on portfolio.

        Args:
            portfolio: Portfolio positions
            scenarios: Stress scenarios to run

        Returns:
            Stress test results
        """
        return {
            "scenarios_tested": len(scenarios),
            "results": {
                "2008_crisis": {
                    "loss_pct": -35.0,
                    "recovery_days": 180,
                    "max_drawdown": -42.0,
                },
                "2020_covid": {
                    "loss_pct": -28.0,
                    "recovery_days": 45,
                    "max_drawdown": -33.0,
                },
                "2022_rate_hike": {
                    "loss_pct": -18.0,
                    "recovery_days": 120,
                    "max_drawdown": -22.0,
                },
                "flash_crash": {
                    "loss_pct": -8.0,
                    "recovery_days": 3,
                    "max_drawdown": -12.0,
                },
                "vol_spike_50pct": {
                    "loss_pct": -15.0,
                    "recovery_days": 30,
                    "max_drawdown": -18.0,
                },
            },
            "worst_case_loss": -42.0,
            "expected_loss_severe": -25.0,
            "stress_capital_required": 50000000,
            "recommendation": "REDUCE_EXPOSURE",
        }

    def analyze_factor_exposure(
        self,
        portfolio: Dict[str, float]
    ) -> Dict[str, Any]:
        """
        Analyze portfolio factor exposures.

        Args:
            portfolio: Portfolio positions

        Returns:
            Factor exposure analysis
        """
        return {
            "factor_exposures": {
                "market_beta": 0.85,
                "size_smb": -0.15,  # Slight large cap bias
                "value_hml": 0.10,
                "momentum_umd": 0.25,
                "quality_qmj": 0.30,
                "low_vol": -0.20,
                "liquidity": 0.05,
            },
            "factor_contributions_to_risk": {
                "market": 0.65,  # 65% of risk from market
                "size": 0.05,
                "value": 0.08,
                "momentum": 0.12,
                "residual": 0.10,
            },
            "factor_tilts_vs_benchmark": {
                "overweight": ["momentum", "quality"],
                "underweight": ["value", "size"],
                "neutral": ["market"],
            },
            "tracking_error_pct": 3.5,
            "active_share": 0.72,
            "factor_crowding_risk": "moderate",
        }

    def calculate_correlation_risk(
        self,
        portfolio: Dict[str, float]
    ) -> Dict[str, Any]:
        """
        Analyze correlation-based risks.

        Args:
            portfolio: Portfolio positions

        Returns:
            Correlation risk metrics
        """
        return {
            "average_correlation": 0.42,
            "correlation_percentile": 0.75,  # Higher than 75% of history
            "correlation_regime": "elevated",
            "diversification_ratio": 1.8,
            "effective_n_assets": 25,  # Out of 100 positions
            "correlation_stress": {
                "if_corr_goes_to_0.8": {
                    "var_increase_pct": 45,
                    "expected_loss_increase": 30,
                },
            },
            "tail_correlation": 0.65,  # Correlation spikes in tails
            "recommendation": "ADD_DIVERSIFIERS",
        }

    def check_concentration(
        self,
        portfolio: Dict[str, float]
    ) -> Dict[str, Any]:
        """
        Check portfolio concentration limits.

        Args:
            portfolio: Portfolio positions

        Returns:
            Concentration analysis
        """
        return {
            "top_5_concentration": 0.25,  # 25% in top 5
            "top_10_concentration": 0.40,
            "sector_concentration": {
                "technology": 0.28,  # 28% in tech
                "healthcare": 0.15,
                "financials": 0.12,
            },
            "single_name_max": 0.035,  # 3.5% max single name
            "limit_breaches": [
                {
                    "type": "sector",
                    "sector": "technology",
                    "exposure": 0.28,
                    "limit": 0.25,
                    "breach_pct": 12,
                }
            ],
            "herfindahl_index": 0.015,
            "gini_coefficient": 0.55,
            "recommendation": "REDUCE_TECH_EXPOSURE",
        }

    def monitor_drawdown(
        self,
        portfolio_value_history: List[float]
    ) -> Dict[str, Any]:
        """
        Monitor portfolio drawdown.

        Args:
            portfolio_value_history: Historical portfolio values

        Returns:
            Drawdown metrics
        """
        return {
            "current_drawdown_pct": -5.2,
            "max_drawdown_pct": -12.5,
            "drawdown_duration_days": 15,
            "days_since_high_water_mark": 18,
            "drawdown_limit": -15.0,
            "limit_utilization": 0.35,
            "average_recovery_time": 25,
            "drawdown_velocity": -0.3,  # Losing 0.3% per day
            "drawdown_percentile": 0.72,  # Worse than 72% of history
            "alert_level": "WARNING",
            "recommendation": "REDUCE_RISK_GRADUALLY",
        }

    def calculate_tail_risk(
        self,
        portfolio: Dict[str, float]
    ) -> Dict[str, Any]:
        """
        Calculate tail risk metrics.

        Args:
            portfolio: Portfolio positions

        Returns:
            Tail risk analysis
        """
        return {
            "expected_shortfall_99": 4.5,  # 4.5% CVaR
            "expected_shortfall_99_9": 7.2,
            "tail_index": 3.5,  # Fat tails
            "skewness": -0.8,  # Negative skew
            "kurtosis": 5.2,  # Fat tails
            "worst_day_historical": -8.5,
            "worst_week_historical": -15.2,
            "black_swan_probability": 0.02,  # 2% chance of extreme event
            "tail_hedging_cost_monthly": 0.15,  # 15 bps
            "recommendation": "CONSIDER_TAIL_HEDGE",
        }

    def assess_liquidity_risk(
        self,
        portfolio: Dict[str, float]
    ) -> Dict[str, Any]:
        """
        Assess portfolio liquidity risk.

        Args:
            portfolio: Portfolio positions

        Returns:
            Liquidity risk assessment
        """
        return {
            "days_to_liquidate_100pct": 5.2,
            "days_to_liquidate_50pct": 1.8,
            "liquidation_cost_bps": 25,
            "illiquid_positions_pct": 0.08,  # 8% illiquid
            "liquidity_score": 0.82,  # 0-1 scale
            "bid_ask_cost": 12,  # bps
            "market_impact_estimate": 35,  # bps for full liquidation
            "liquidity_bucketing": {
                "highly_liquid": 0.65,
                "liquid": 0.25,
                "semi_liquid": 0.08,
                "illiquid": 0.02,
            },
            "liquidity_stress_scenario": {
                "cost_in_stress": 75,  # bps
                "time_to_liquidate_stress": 12,  # days
            },
            "recommendation": "MAINTAIN_LIQUIDITY_BUFFER",
        }
