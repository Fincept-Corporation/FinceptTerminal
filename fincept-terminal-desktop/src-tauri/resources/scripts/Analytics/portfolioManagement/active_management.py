
"""Portfolio Active Management Module
===============================

Active portfolio management strategies

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
from enum import Enum
import warnings

from config import MathConstants, validate_returns, ERROR_MESSAGES
from math_engine import StatisticalCalculations, PerformanceCalculations


class ActiveStrategy(Enum):
    """Types of active management strategies"""
    MARKET_TIMING = "market_timing"
    SECURITY_SELECTION = "security_selection"
    SECTOR_ROTATION = "sector_rotation"
    STYLE_TILTING = "style_tilting"
    FACTOR_TIMING = "factor_timing"


class FactorModelType(Enum):
    """Types of factor models"""
    MACROECONOMIC = "macroeconomic"
    FUNDAMENTAL = "fundamental"
    STATISTICAL = "statistical"


@dataclass
class ActiveManagementMetrics:
    """Active management performance metrics"""
    active_return: float
    tracking_error: float
    information_ratio: float
    hit_rate: float
    active_risk: float
    transfer_coefficient: float


class ValueAddedMeasurement:
    """Measurement of value added by active management"""

    @staticmethod
    def calculate_value_added(portfolio_returns: np.ndarray,
                              benchmark_returns: np.ndarray,
                              portfolio_weights: Optional[np.ndarray] = None) -> Dict:
        """Calculate value added by active management"""

        if not validate_returns(portfolio_returns) or not validate_returns(benchmark_returns):
            raise ValueError(ERROR_MESSAGES["insufficient_data"])

        # Active returns (excess returns over benchmark)
        active_returns = np.array(portfolio_returns) - np.array(benchmark_returns)

        # Basic value added metrics
        mean_active_return = np.mean(active_returns)
        tracking_error = np.std(active_returns, ddof=1)

        # Annualize metrics
        annualized_active_return = mean_active_return * MathConstants.TRADING_DAYS_YEAR
        annualized_tracking_error = tracking_error * MathConstants.SQRT_TRADING_DAYS

        # Information ratio
        information_ratio = mean_active_return / tracking_error if tracking_error > 0 else 0
        annualized_ir = information_ratio * MathConstants.SQRT_TRADING_DAYS

        # Hit rate (percentage of periods with positive active returns)
        hit_rate = np.sum(active_returns > 0) / len(active_returns)

        # Value added decomposition
        value_decomposition = ValueAddedMeasurement._decompose_value_added(
            portfolio_returns, benchmark_returns, portfolio_weights
        )

        return {
            "active_return_daily": mean_active_return,
            "active_return_annualized": annualized_active_return,
            "tracking_error_daily": tracking_error,
            "tracking_error_annualized": annualized_tracking_error,
            "information_ratio_daily": information_ratio,
            "information_ratio_annualized": annualized_ir,
            "hit_rate": hit_rate,
            "value_added_decomposition": value_decomposition,
            "statistical_significance": ValueAddedMeasurement._test_statistical_significance(
                active_returns
            )
        }

    @staticmethod
    def _decompose_value_added(portfolio_returns: np.ndarray,
                               benchmark_returns: np.ndarray,
                               portfolio_weights: Optional[np.ndarray]) -> Dict:
        """Decompose value added into sources"""

        # Simplified decomposition - in practice would need detailed holdings data
        total_active_return = np.mean(portfolio_returns) - np.mean(benchmark_returns)

        # Estimate allocation vs selection effects (simplified)
        # This would require sector/security level data in practice
        estimated_allocation_effect = total_active_return * 0.3  # Rough estimate
        estimated_selection_effect = total_active_return * 0.7  # Rough estimate

        return {
            "total_active_return": total_active_return * MathConstants.TRADING_DAYS_YEAR,
            "estimated_allocation_effect": estimated_allocation_effect * MathConstants.TRADING_DAYS_YEAR,
            "estimated_selection_effect": estimated_selection_effect * MathConstants.TRADING_DAYS_YEAR,
            "interaction_effect": 0.0,  # Would calculate with detailed data
            "note": "Decomposition requires detailed holdings data for precision"
        }

    @staticmethod
    def _test_statistical_significance(active_returns: np.ndarray) -> Dict:
        """Test statistical significance of active returns"""

        mean_active = np.mean(active_returns)
        std_active = np.std(active_returns, ddof=1)
        n_periods = len(active_returns)

        # T-statistic for testing if mean active return is significantly different from zero
        t_statistic = mean_active / (std_active / np.sqrt(n_periods)) if std_active > 0 else 0

        # Critical values (approximate)
        critical_90 = 1.645
        critical_95 = 1.96
        critical_99 = 2.576

        significance_level = "Not significant"
        if abs(t_statistic) > critical_99:
            significance_level = "99% significant"
        elif abs(t_statistic) > critical_95:
            significance_level = "95% significant"
        elif abs(t_statistic) > critical_90:
            significance_level = "90% significant"

        return {
            "t_statistic": t_statistic,
            "significance_level": significance_level,
            "is_significant_95": abs(t_statistic) > critical_95,
            "sample_size": n_periods,
            "standard_error": std_active / np.sqrt(n_periods)
        }


class InformationRatioAnalysis:
    """Information ratio calculation and analysis"""

    @staticmethod
    def calculate_information_ratio_ex_post(portfolio_returns: np.ndarray,
                                            benchmark_returns: np.ndarray) -> Dict:
        """Calculate ex-post (realized) information ratio"""

        active_returns = np.array(portfolio_returns) - np.array(benchmark_returns)

        mean_active_return = np.mean(active_returns)
        tracking_error = np.std(active_returns, ddof=1)

        # Information ratio
        ir_daily = mean_active_return / tracking_error if tracking_error > 0 else 0
        ir_annualized = ir_daily * MathConstants.SQRT_TRADING_DAYS

        # Compare to Sharpe ratio
        portfolio_sharpe = PerformanceCalculations.sharpe_ratio(portfolio_returns)
        benchmark_sharpe = PerformanceCalculations.sharpe_ratio(benchmark_returns)

        return {
            "information_ratio_daily": ir_daily,
            "information_ratio_annualized": ir_annualized,
            "active_return_annualized": mean_active_return * MathConstants.TRADING_DAYS_YEAR,
            "tracking_error_annualized": tracking_error * MathConstants.SQRT_TRADING_DAYS,
            "portfolio_sharpe_ratio": portfolio_sharpe,
            "benchmark_sharpe_ratio": benchmark_sharpe,
            "sharpe_ratio_difference": portfolio_sharpe - benchmark_sharpe,
            "ir_quality_assessment": InformationRatioAnalysis._assess_ir_quality(ir_annualized)
        }

    @staticmethod
    def calculate_information_ratio_ex_ante(expected_active_return: float,
                                            expected_tracking_error: float,
                                            forecast_horizon_months: int = 12) -> Dict:
        """Calculate ex-ante (expected) information ratio"""

        # Convert to appropriate time horizon
        time_factor = np.sqrt(forecast_horizon_months / 12)

        ex_ante_ir = expected_active_return / expected_tracking_error if expected_tracking_error > 0 else 0

        # Confidence intervals for ex-ante IR
        confidence_intervals = InformationRatioAnalysis._calculate_ir_confidence_intervals(
            ex_ante_ir, forecast_horizon_months
        )

        return {
            "ex_ante_information_ratio": ex_ante_ir,
            "expected_active_return": expected_active_return,
            "expected_tracking_error": expected_tracking_error,
            "forecast_horizon_months": forecast_horizon_months,
            "confidence_intervals": confidence_intervals,
            "probability_of_positive_alpha": InformationRatioAnalysis._probability_positive_alpha(
                expected_active_return, expected_tracking_error
            )
        }

    @staticmethod
    def compare_ir_to_sharpe(information_ratio: float,
                             portfolio_sharpe: float,
                             benchmark_sharpe: float) -> Dict:
        """Compare Information Ratio to Sharpe ratios"""

        # Theoretical relationship: IR measures active management skill
        # while Sharpe measures total risk-adjusted return

        analysis = {
            "information_ratio": information_ratio,
            "portfolio_sharpe": portfolio_sharpe,
            "benchmark_sharpe": benchmark_sharpe,
            "interpretation": {
                "ir_focus": "Measures excess return per unit of active risk",
                "sharpe_focus": "Measures excess return per unit of total risk",
                "complementary_nature": "Both metrics provide different insights into performance"
            }
        }

        # Practical interpretation
        if information_ratio > 0.5:
            analysis["ir_assessment"] = "Excellent active management"
        elif information_ratio > 0.25:
            analysis["ir_assessment"] = "Good active management"
        elif information_ratio > 0:
            analysis["ir_assessment"] = "Modest active management value"
        else:
            analysis["ir_assessment"] = "Value-destroying active management"

        return analysis

    @staticmethod
    def _assess_ir_quality(ir_annualized: float) -> Dict:
        """Assess quality of information ratio"""

        if ir_annualized > 0.75:
            quality = "Exceptional"
            percentile = "Top 5%"
        elif ir_annualized > 0.50:
            quality = "Excellent"
            percentile = "Top 10%"
        elif ir_annualized > 0.25:
            quality = "Good"
            percentile = "Top 25%"
        elif ir_annualized > 0:
            quality = "Below average"
            percentile = "Below median"
        else:
            quality = "Poor"
            percentile = "Bottom quartile"

        return {
            "quality_rating": quality,
            "industry_percentile": percentile,
            "sustainability_concern": ir_annualized > 1.0  # Very high IRs often not sustainable
        }

    @staticmethod
    def _calculate_ir_confidence_intervals(ir: float, horizon_months: int) -> Dict:
        """Calculate confidence intervals for information ratio"""

        # Standard error of IR (simplified)
        n_observations = horizon_months * 21  # Approximate daily observations
        ir_standard_error = np.sqrt((1 + ir ** 2 / 2) / n_observations)

        # 95% confidence interval
        margin_of_error = 1.96 * ir_standard_error

        return {
            "95_percent_confidence_interval": {
                "lower_bound": ir - margin_of_error,
                "upper_bound": ir + margin_of_error
            },
            "standard_error": ir_standard_error,
            "sample_size_effect": "Longer track records provide more reliable IR estimates"
        }

    @staticmethod
    def _probability_positive_alpha(expected_return: float, tracking_error: float) -> float:
        """Calculate probability of generating positive alpha"""

        if tracking_error <= 0:
            return 0.5

        # Assuming normal distribution
        from scipy import stats
        z_score = expected_return / tracking_error
        probability = stats.norm.cdf(z_score)

        return probability


class FundamentalLawActiveManagement:
    """Fundamental Law of Active Management implementation"""

    @staticmethod
    def calculate_fundamental_law_components(portfolio_data: Dict) -> Dict:
        """Calculate components of the Fundamental Law of Active Management"""

        # Extract components
        information_coefficient = portfolio_data.get("information_coefficient", 0.05)
        breadth = portfolio_data.get("breadth", 100)
        transfer_coefficient = portfolio_data.get("transfer_coefficient", 0.8)
        active_risk = portfolio_data.get("active_risk", 0.04)

        # Calculate Information Ratio using Fundamental Law
        # IR = IC × √Breadth × TC
        fundamental_law_ir = information_coefficient * np.sqrt(breadth) * transfer_coefficient

        # Expected active return
        expected_active_return = fundamental_law_ir * active_risk

        return {
            "information_coefficient": information_coefficient,
            "breadth": breadth,
            "transfer_coefficient": transfer_coefficient,
            "active_risk": active_risk,
            "fundamental_law_ir": fundamental_law_ir,
            "expected_active_return": expected_active_return,
            "component_analysis": FundamentalLawActiveManagement._analyze_components(
                information_coefficient, breadth, transfer_coefficient
            )
        }

    @staticmethod
    def analyze_strategy_changes(current_strategy: Dict, proposed_strategy: Dict) -> Dict:
        """Analyze strategy changes in terms of Fundamental Law"""

        # Current strategy metrics
        current_ic = current_strategy.get("information_coefficient", 0.05)
        current_breadth = current_strategy.get("breadth", 100)
        current_tc = current_strategy.get("transfer_coefficient", 0.8)
        current_ir = current_ic * np.sqrt(current_breadth) * current_tc

        # Proposed strategy metrics
        proposed_ic = proposed_strategy.get("information_coefficient", 0.05)
        proposed_breadth = proposed_strategy.get("breadth", 100)
        proposed_tc = proposed_strategy.get("transfer_coefficient", 0.8)
        proposed_ir = proposed_ic * np.sqrt(proposed_breadth) * proposed_tc

        # Analysis of changes
        ir_change = proposed_ir - current_ir
        ir_change_pct = (ir_change / current_ir * 100) if current_ir != 0 else 0

        # Component contributions to change
        component_contributions = FundamentalLawActiveManagement._decompose_ir_change(
            current_strategy, proposed_strategy
        )

        return {
            "current_strategy_ir": current_ir,
            "proposed_strategy_ir": proposed_ir,
            "ir_change": ir_change,
            "ir_change_percentage": ir_change_pct,
            "component_contributions": component_contributions,
            "recommendation": FundamentalLawActiveManagement._generate_strategy_recommendation(
                ir_change, component_contributions
            )
        }

    @staticmethod
    def market_timing_vs_security_selection(market_timing_data: Dict,
                                            security_selection_data: Dict) -> Dict:
        """Compare market timing vs security selection strategies"""

        # Market timing analysis
        mt_ic = market_timing_data.get("information_coefficient", 0.1)  # Higher IC, lower breadth
        mt_breadth = market_timing_data.get("breadth", 12)  # Monthly decisions
        mt_tc = market_timing_data.get("transfer_coefficient", 0.9)
        mt_ir = mt_ic * np.sqrt(mt_breadth) * mt_tc

        # Security selection analysis
        ss_ic = security_selection_data.get("information_coefficient", 0.03)  # Lower IC, higher breadth
        ss_breadth = security_selection_data.get("breadth", 500)  # Many securities
        ss_tc = security_selection_data.get("transfer_coefficient", 0.7)
        ss_ir = ss_ic * np.sqrt(ss_breadth) * ss_tc

        comparison = {
            "market_timing": {
                "information_coefficient": mt_ic,
                "breadth": mt_breadth,
                "transfer_coefficient": mt_tc,
                "information_ratio": mt_ir,
                "characteristics": "High skill, low breadth, high implementation efficiency"
            },
            "security_selection": {
                "information_coefficient": ss_ic,
                "breadth": ss_breadth,
                "transfer_coefficient": ss_tc,
                "information_ratio": ss_ir,
                "characteristics": "Moderate skill, high breadth, good implementation"
            },
            "comparison_insights": {
                "higher_ir_strategy": "Market Timing" if mt_ir > ss_ir else "Security Selection",
                "breadth_vs_skill_tradeoff": "Market timing requires higher skill but offers lower breadth",
                "implementation_challenges": FundamentalLawActiveManagement._compare_implementation_challenges()
            }
        }

        return comparison

    @staticmethod
    def _analyze_components(ic: float, breadth: float, tc: float) -> Dict:
        """Analyze individual components of the Fundamental Law"""

        return {
            "information_coefficient_analysis": {
                "value": ic,
                "interpretation": "High" if ic > 0.1 else "Moderate" if ic > 0.05 else "Low",
                "improvement_potential": "Skill development, better research, improved models",
                "typical_range": "0.02 to 0.15 for most active managers"
            },
            "breadth_analysis": {
                "value": breadth,
                "interpretation": "High" if breadth > 200 else "Moderate" if breadth > 50 else "Low",
                "improvement_potential": "Expand universe, increase decision frequency",
                "trade_offs": "More breadth may reduce IC if skill is diluted"
            },
            "transfer_coefficient_analysis": {
                "value": tc,
                "interpretation": "High" if tc > 0.8 else "Moderate" if tc > 0.6 else "Low",
                "improvement_potential": "Reduce constraints, improve implementation",
                "constraints_impact": "Benchmarking, risk limits, liquidity affect TC"
            }
        }

    @staticmethod
    def _decompose_ir_change(current: Dict, proposed: Dict) -> Dict:
        """Decompose change in IR into component contributions"""

        # Calculate marginal contribution of each component
        # This is a simplified linear approximation

        current_ic = current.get("information_coefficient", 0.05)
        current_breadth = current.get("breadth", 100)
        current_tc = current.get("transfer_coefficient", 0.8)

        proposed_ic = proposed.get("information_coefficient", 0.05)
        proposed_breadth = proposed.get("breadth", 100)
        proposed_tc = proposed.get("transfer_coefficient", 0.8)

        # Marginal contributions (approximate)
        ic_contribution = (proposed_ic - current_ic) * np.sqrt(current_breadth) * current_tc
        breadth_contribution = current_ic * (np.sqrt(proposed_breadth) - np.sqrt(current_breadth)) * current_tc
        tc_contribution = current_ic * np.sqrt(current_breadth) * (proposed_tc - current_tc)

        return {
            "ic_contribution": ic_contribution,
            "breadth_contribution": breadth_contribution,
            "tc_contribution": tc_contribution,
            "total_change": ic_contribution + breadth_contribution + tc_contribution,
            "largest_contributor": max([
                ("IC", ic_contribution),
                ("Breadth", breadth_contribution),
                ("TC", tc_contribution)
            ], key=lambda x: abs(x[1]))[0]
        }


class MultifactorModels:
    """Multifactor models implementation and analysis"""

    @staticmethod
    def arbitrage_pricing_theory_analysis() -> Dict:
        """Analyze Arbitrage Pricing Theory and its assumptions"""

        return {
            "apt_assumptions": {
                "factor_model": "Returns generated by linear factor model",
                "diversification": "Unsystematic risk can be diversified away",
                "no_arbitrage": "No arbitrage opportunities exist in equilibrium",
                "competitive_markets": "Markets are competitive with rational investors"
            },
            "apt_equation": {
                "mathematical_form": "E(Ri) = λ0 + λ1*βi1 + λ2*βi2 + ... + λk*βik",
                "components": {
                    "E(Ri)": "Expected return on asset i",
                    "λ0": "Risk-free rate or factor risk premium for λ0",
                    "λj": "Risk premium for factor j",
                    "βij": "Sensitivity of asset i to factor j"
                }
            },
            "relationship_to_multifactor_models": {
                "apt_foundation": "APT provides theoretical foundation for multifactor models",
                "practical_implementation": "Multifactor models are practical implementations of APT",
                "factor_identification": "APT doesn't specify factors; models must identify relevant factors"
            },
            "advantages": [
                "More general than CAPM",
                "Allows for multiple risk factors",
                "No specific utility function required",
                "Fewer restrictive assumptions than CAPM"
            ],
            "limitations": [
                "Doesn't specify which factors to use",
                "Factor risk premiums must be estimated",
                "Requires large portfolios for diversification assumption",
                "Testing APT is challenging empirically"
            ]
        }

    @staticmethod
    def identify_arbitrage_opportunity(expected_returns: np.ndarray,
                                       factor_loadings: np.ndarray,
                                       factor_risk_premiums: np.ndarray,
                                       risk_free_rate: float = 0.03) -> Dict:
        """Identify arbitrage opportunities using APT"""

        # Calculate APT-implied expected returns
        apt_expected_returns = risk_free_rate + np.dot(factor_loadings, factor_risk_premiums)

        # Compare with market expected returns
        return_differences = expected_returns - apt_expected_returns

        # Identify potential arbitrage opportunities
        arbitrage_opportunities = []

        for i, diff in enumerate(return_differences):
            if abs(diff) > 0.005:  # 0.5% threshold for significance
                arbitrage_opportunities.append({
                    "asset_index": i,
                    "expected_return": expected_returns[i],
                    "apt_fair_value": apt_expected_returns[i],
                    "difference": diff,
                    "direction": "Overvalued" if diff < 0 else "Undervalued",
                    "arbitrage_potential": abs(diff)
                })

        return {
            "arbitrage_opportunities": arbitrage_opportunities,
            "number_of_opportunities": len(arbitrage_opportunities),
            "largest_opportunity": max(arbitrage_opportunities,
                                       key=lambda x: x["arbitrage_potential"]) if arbitrage_opportunities else None,
            "market_efficiency_assessment": MultifactorModels._assess_market_efficiency(return_differences)
        }

    @staticmethod
    def calculate_expected_return_multifactor(factor_sensitivities: np.ndarray,
                                              factor_risk_premiums: np.ndarray,
                                              risk_free_rate: float = 0.03) -> Dict:
        """Calculate expected return using multifactor model"""

        # E(R) = Rf + β1*RP1 + β2*RP2 + ... + βk*RPk
        factor_contributions = factor_sensitivities * factor_risk_premiums
        total_risk_premium = np.sum(factor_contributions)
        expected_return = risk_free_rate + total_risk_premium

        return {
            "expected_return": expected_return,
            "risk_free_rate": risk_free_rate,
            "total_risk_premium": total_risk_premium,
            "factor_contributions": {
                f"factor_{i + 1}": {
                    "sensitivity": factor_sensitivities[i],
                    "risk_premium": factor_risk_premiums[i],
                    "contribution": factor_contributions[i]
                }
                for i in range(len(factor_sensitivities))
            },
            "decomposition_analysis": MultifactorModels._analyze_return_decomposition(
                factor_contributions, total_risk_premium
            )
        }

    @staticmethod
    def compare_factor_models() -> Dict:
        """Compare different types of factor models"""

        return {
            "macroeconomic_factor_models": {
                "description": "Use observable economic variables as factors",
                "examples": ["GDP growth", "Inflation", "Interest rates", "Oil prices"],
                "advantages": [
                    "Intuitive economic interpretation",
                    "Factors are observable",
                    "Good for scenario analysis",
                    "Useful for risk management"
                ],
                "disadvantages": [
                    "May miss important risk factors",
                    "Economic factors may be correlated",
                    "Factor loadings must be estimated",
                    "Limited number of factors"
                ],
                "typical_applications": ["Asset allocation", "Risk management", "Scenario analysis"]
            },
            "fundamental_factor_models": {
                "description": "Use company/security fundamental attributes as factors",
                "examples": ["P/E ratio", "Book-to-market", "Market cap", "Earnings growth"],
                "advantages": [
                    "Factors directly observable from financial data",
                    "Good for security selection",
                    "Many factors can be included",
                    "Useful for portfolio construction"
                ],
                "disadvantages": [
                    "Factor returns must be estimated",
                    "May have multicollinearity issues",
                    "Requires frequent factor updating",
                    "Survivorship bias in factor construction"
                ],
                "typical_applications": ["Security selection", "Portfolio construction", "Performance attribution"]
            },
            "statistical_factor_models": {
                "description": "Use statistical techniques to extract factors from return data",
                "examples": ["Principal component analysis", "Factor analysis", "Independent component analysis"],
                "advantages": [
                    "Factors explain maximum return variation",
                    "No need to specify factors a priori",
                    "Factors are orthogonal",
                    "Good statistical properties"
                ],
                "disadvantages": [
                    "Factors lack economic interpretation",
                    "Factors may be unstable over time",
                    "Difficult to use for forecasting",
                    "Black box approach"
                ],
                "typical_applications": ["Risk modeling", "Portfolio optimization", "Performance measurement"]
            }
        }

    @staticmethod
    def analyze_multifactor_model_uses(portfolio_data: Dict) -> Dict:
        """Analyze uses of multifactor models"""

        uses = {
            "risk_management": {
                "description": "Decompose portfolio risk into factor exposures",
                "applications": [
                    "Track factor exposures relative to benchmark",
                    "Identify concentrated factor bets",
                    "Stress test portfolio under factor scenarios",
                    "Set factor exposure limits"
                ],
                "example_analysis": MultifactorModels._example_risk_decomposition(portfolio_data)
            },
            "portfolio_construction": {
                "description": "Build portfolios with desired factor exposures",
                "applications": [
                    "Optimize portfolio for factor exposures",
                    "Create factor-neutral portfolios",
                    "Implement factor tilts",
                    "Control unintended factor bets"
                ],
                "optimization_considerations": [
                    "Transaction costs and turnover",
                    "Factor exposure constraints",
                    "Risk budgeting across factors",
                    "Implementation efficiency"
                ]
            },
            "performance_attribution": {
                "description": "Attribute portfolio performance to factor exposures",
                "applications": [
                    "Separate alpha from factor returns",
                    "Identify sources of outperformance",
                    "Evaluate manager skill vs factor timing",
                    "Benchmark factor-adjusted performance"
                ],
                "attribution_framework": MultifactorModels._performance_attribution_framework()
            }
        }

        return uses

    @staticmethod
    def multiple_risk_dimensions_benefits() -> Dict:
        """Describe benefits of considering multiple risk dimensions"""

        return {
            "comprehensive_risk_capture": {
                "single_factor_limitations": "Single factor (like market beta) may miss important risks",
                "multifactor_advantages": "Captures various sources of systematic risk",
                "risk_factor_examples": [
                    "Market risk (beta)",
                    "Size risk (small vs large cap)",
                    "Value risk (value vs growth)",
                    "Momentum risk",
                    "Quality risk",
                    "Volatility risk"
                ]
            },
            "improved_risk_measurement": {
                "better_diversification_assessment": "Identifies true diversification vs correlated factor exposures",
                "concentrated_risk_identification": "Reveals hidden concentrations in factor exposures",
                "stress_testing_enhancement": "Enables factor-based scenario analysis",
                "tail_risk_modeling": "Better modeling of extreme events through factor interactions"
            },
            "enhanced_portfolio_construction": {
                "factor_based_allocation": "Allocate risk budget across multiple factors",
                "smart_beta_implementation": "Systematic factor exposure implementation",
                "alpha_separation": "Separate factor returns from true alpha generation",
                "benchmark_relative_positioning": "Control factor exposures relative to benchmark"
            },
            "practical_applications": {
                "asset_allocation": "Factor-based strategic asset allocation",
                "security_selection": "Factor-aware security selection process",
                "risk_budgeting": "Allocate risk across factors rather than just assets",
                "performance_evaluation": "Factor-adjusted performance measurement"
            }
        }

    @staticmethod
    def _assess_market_efficiency(return_differences: np.ndarray) -> Dict:
        """Assess market efficiency based on APT deviations"""

        abs_differences = np.abs(return_differences)
        mean_abs_difference = np.mean(abs_differences)
        max_abs_difference = np.max(abs_differences)

        if mean_abs_difference < 0.01:
            efficiency = "High"
            assessment = "Market appears highly efficient"
        elif mean_abs_difference < 0.02:
            efficiency = "Moderate"
            assessment = "Some inefficiencies present"
        else:
            efficiency = "Low"
            assessment = "Significant inefficiencies detected"

        return {
            "efficiency_level": efficiency,
            "assessment": assessment,
            "mean_absolute_deviation": mean_abs_difference,
            "maximum_deviation": max_abs_difference,
            "number_of_large_deviations": np.sum(abs_differences > 0.02)
        }


class ActiveRiskTracking:
    """Active risk sources and tracking analysis"""

    @staticmethod
    def decompose_active_risk(portfolio_weights: np.ndarray,
                              benchmark_weights: np.ndarray,
                              factor_exposures: np.ndarray,
                              factor_covariance: np.ndarray,
                              specific_risk: np.ndarray) -> Dict:
        """Decompose active risk into factor and specific components"""

        # Active weights
        active_weights = portfolio_weights - benchmark_weights

        # Factor risk contribution
        portfolio_factor_exposure = np.dot(active_weights, factor_exposures)
        factor_risk = np.sqrt(np.dot(portfolio_factor_exposure,
                                     np.dot(factor_covariance, portfolio_factor_exposure)))

        # Specific risk contribution
        specific_risk_contribution = np.sqrt(np.dot(active_weights ** 2, specific_risk ** 2))

        # Total active risk (simplified - ignoring factor-specific correlation)
        total_active_risk = np.sqrt(factor_risk ** 2 + specific_risk_contribution ** 2)

        return {
            "total_active_risk": total_active_risk,
            "factor_risk_contribution": factor_risk,
            "specific_risk_contribution": specific_risk_contribution,
            "factor_risk_percentage": (factor_risk / total_active_risk) * 100 if total_active_risk > 0 else 0,
            "specific_risk_percentage": (
                                                    specific_risk_contribution / total_active_risk) * 100 if total_active_risk > 0 else 0,
            "risk_decomposition_analysis": ActiveRiskTracking._analyze_risk_decomposition(
                factor_risk, specific_risk_contribution, total_active_risk
            )
        }

    @staticmethod
    def calculate_tracking_risk(portfolio_returns: np.ndarray,
                                benchmark_returns: np.ndarray,
                                rolling_window: int = 252) -> Dict:
        """Calculate tracking risk with rolling analysis"""

        active_returns = portfolio_returns - benchmark_returns

        # Overall tracking risk
        tracking_risk = np.std(active_returns, ddof=1) * MathConstants.SQRT_TRADING_DAYS

        # Rolling tracking risk
        rolling_tracking_risk = []
        for i in range(rolling_window, len(active_returns)):
            window_returns = active_returns[i - rolling_window:i]
            rolling_te = np.std(window_returns, ddof=1) * MathConstants.SQRT_TRADING_DAYS
            rolling_tracking_risk.append(rolling_te)

        rolling_tracking_risk = np.array(rolling_tracking_risk)

        return {
            "tracking_risk_annualized": tracking_risk,
            "rolling_tracking_risk": rolling_tracking_risk,
            "tracking_risk_statistics": {
                "mean_rolling_te": np.mean(rolling_tracking_risk),
                "std_rolling_te": np.std(rolling_tracking_risk),
                "min_rolling_te": np.min(rolling_tracking_risk),
                "max_rolling_te": np.max(rolling_tracking_risk)
            },
            "tracking_risk_stability": ActiveRiskTracking._assess_tracking_risk_stability(
                rolling_tracking_risk
            ),
            "active_return_analysis": ActiveRiskTracking._analyze_active_returns(active_returns)
        }

    @staticmethod
    def sources_of_active_risk() -> Dict:
        """Identify and explain sources of active risk"""

        return {
            "asset_allocation_risk": {
                "description": "Risk from deviating from benchmark asset allocation",
                "sources": [
                    "Sector allocation differences",
                    "Geographic allocation differences",
                    "Style allocation differences",
                    "Market cap allocation differences"
                ],
                "measurement": "Track active weights in each allocation dimension",
                "management": "Set allocation limits and monitor regularly"
            },
            "security_selection_risk": {
                "description": "Risk from selecting different securities within allocations",
                "sources": [
                    "Stock picking within sectors",
                    "Bond selection within duration/credit buckets",
                    "Overweight/underweight individual positions",
                    "Security-specific events"
                ],
                "measurement": "Analyze stock-specific active weights and performance",
                "management": "Diversification limits and position sizing rules"
            },
            "factor_exposure_risk": {
                "description": "Risk from different factor exposures vs benchmark",
                "sources": [
                    "Value vs growth tilt",
                    "Size factor exposure",
                    "Quality factor exposure",
                    "Momentum factor exposure",
                    "Volatility factor exposure"
                ],
                "measurement": "Factor exposure analysis vs benchmark",
                "management": "Factor exposure limits and hedging"
            },
            "timing_risk": {
                "description": "Risk from timing allocation and selection decisions",
                "sources": [
                    "Market timing attempts",
                    "Sector rotation timing",
                    "Entry/exit timing for positions",
                    "Rebalancing timing"
                ],
                "measurement": "Performance attribution across time periods",
                "management": "Systematic processes and reduced discretionary timing"
            }
        }

    @staticmethod
    def _analyze_risk_decomposition(factor_risk: float, specific_risk: float,
                                    total_risk: float) -> Dict:
        """Analyze the risk decomposition results"""

        factor_dominance = factor_risk > specific_risk

        analysis = {
            "dominant_risk_source": "Factor Risk" if factor_dominance else "Specific Risk",
            "risk_concentration": "High" if max(factor_risk, specific_risk) / total_risk > 0.8 else "Moderate",
            "diversification_effectiveness": "Good" if min(factor_risk, specific_risk) / total_risk > 0.2 else "Poor"
        }

        if factor_dominance:
            analysis["implications"] = [
                "Portfolio risk driven by systematic factor exposures",
                "Consider factor hedging to reduce risk",
                "Factor timing may be significant component of returns"
            ]
        else:
            analysis["implications"] = [
                "Portfolio risk driven by security-specific factors",
                "Diversification may help reduce risk",
                "Stock selection appears to be primary focus"
            ]

        return analysis

    @staticmethod
    def _assess_tracking_risk_stability(rolling_tracking_risk: np.ndarray) -> Dict:
        """Assess stability of tracking risk over time"""

        coefficient_of_variation = np.std(rolling_tracking_risk) / np.mean(rolling_tracking_risk)

        if coefficient_of_variation < 0.2:
            stability = "High"
            assessment = "Tracking risk is very stable"
        elif coefficient_of_variation < 0.4:
            stability = "Moderate"
            assessment = "Tracking risk shows some variation"
        else:
            stability = "Low"
            assessment = "Tracking risk is highly variable"

        return {
            "stability_level": stability,
            "assessment": assessment,
            "coefficient_of_variation": coefficient_of_variation,
            "trend_analysis": "Increasing" if rolling_tracking_risk[-1] > rolling_tracking_risk[0] else "Decreasing"
        }

    @staticmethod
    def _analyze_active_returns(active_returns: np.ndarray) -> Dict:
        """Analyze active return characteristics"""

        return {
            "mean_active_return": np.mean(active_returns) * MathConstants.TRADING_DAYS_YEAR,
            "active_return_volatility": np.std(active_returns, ddof=1) * MathConstants.SQRT_TRADING_DAYS,
            "active_return_skewness": ActiveRiskTracking._calculate_skewness(active_returns),
            "hit_rate": np.sum(active_returns > 0) / len(active_returns),
            "up_capture": np.mean(active_returns[active_returns > 0]) if np.any(active_returns > 0) else 0,
            "down_capture": np.mean(active_returns[active_returns < 0]) if np.any(active_returns < 0) else 0
        }

    @staticmethod
    def _calculate_skewness(data: np.ndarray) -> float:
        """Calculate skewness of data"""
        mean = np.mean(data)
        std = np.std(data, ddof=1)
        n = len(data)

        if std == 0:
            return 0

        skewness = n / ((n - 1) * (n - 2)) * np.sum(((data - mean) / std) ** 3)
        return skewness


class StrategyCombination:
    """Analysis of combining active management strategies"""

    @staticmethod
    def analyze_strategy_combination(strategy1_data: Dict, strategy2_data: Dict,
                                     correlation: float = 0.3) -> Dict:
        """Analyze combination of two active management strategies"""

        # Extract strategy metrics
        s1_return = strategy1_data.get("expected_return", 0.02)
        s1_risk = strategy1_data.get("tracking_error", 0.04)
        s1_ir = s1_return / s1_risk if s1_risk > 0 else 0

        s2_return = strategy2_data.get("expected_return", 0.015)
        s2_risk = strategy2_data.get("tracking_error", 0.03)
        s2_ir = s2_return / s2_risk if s2_risk > 0 else 0

        # Equal weight combination (can be optimized)
        weight1 = 0.5
        weight2 = 0.5

        # Combined portfolio metrics
        combined_return = weight1 * s1_return + weight2 * s2_return
        combined_variance = (weight1 * s1_risk) ** 2 + (
                    weight2 * s2_risk) ** 2 + 2 * weight1 * weight2 * correlation * s1_risk * s2_risk
        combined_risk = np.sqrt(combined_variance)
        combined_ir = combined_return / combined_risk if combined_risk > 0 else 0

        # Optimal weights for maximum IR
        optimal_weights = StrategyCombination._calculate_optimal_weights(
            s1_return, s1_risk, s2_return, s2_risk, correlation
        )

        return {
            "individual_strategies": {
                "strategy1": {"return": s1_return, "risk": s1_risk, "ir": s1_ir},
                "strategy2": {"return": s2_return, "risk": s2_risk, "ir": s2_ir}
            },
            "equal_weight_combination": {
                "weights": [weight1, weight2],
                "expected_return": combined_return,
                "tracking_error": combined_risk,
                "information_ratio": combined_ir
            },
            "optimal_combination": optimal_weights,
            "correlation_impact": StrategyCombination._analyze_correlation_impact(
                s1_risk, s2_risk, correlation
            ),
            "diversification_benefit": StrategyCombination._calculate_diversification_benefit(
                s1_risk, s2_risk, combined_risk, weight1, weight2
            )
        }

    @staticmethod
    def _calculate_optimal_weights(return1: float, risk1: float, return2: float,
                                   risk2: float, correlation: float) -> Dict:
        """Calculate optimal weights for maximum Information Ratio"""

        # Covariance matrix
        cov_matrix = np.array([
            [risk1 ** 2, correlation * risk1 * risk2],
            [correlation * risk1 * risk2, risk2 ** 2]
        ])

        # Expected returns vector
        returns = np.array([return1, return2])

        # Calculate optimal weights (maximize IR = w'μ / sqrt(w'Σw))
        try:
            inv_cov = np.linalg.inv(cov_matrix)
            optimal_weights = np.dot(inv_cov, returns)
            optimal_weights = optimal_weights / np.sum(optimal_weights)  # Normalize to sum to 1

            # Calculate optimal portfolio metrics
            opt_return = np.dot(optimal_weights, returns)
            opt_variance = np.dot(optimal_weights, np.dot(cov_matrix, optimal_weights))
            opt_risk = np.sqrt(opt_variance)
            opt_ir = opt_return / opt_risk if opt_risk > 0 else 0

            return {
                "optimal_weights": optimal_weights.tolist(),
                "optimal_return": opt_return,
                "optimal_risk": opt_risk,
                "optimal_ir": opt_ir,
                "improvement_vs_equal_weight": opt_ir - (opt_return / opt_risk if opt_risk > 0 else 0)
            }

        except np.linalg.LinAlgError:
            return {
                "optimal_weights": [0.5, 0.5],
                "note": "Singular covariance matrix - using equal weights"
            }

    @staticmethod
    def _analyze_correlation_impact(risk1: float, risk2: float, correlation: float) -> Dict:
        """Analyze impact of correlation on strategy combination"""

        # Risk reduction scenarios
        scenarios = {}
        for corr in [-0.5, 0.0, 0.5, 1.0]:
            combined_variance = 0.25 * risk1 ** 2 + 0.25 * risk2 ** 2 + 2 * 0.5 * 0.5 * corr * risk1 * risk2
            combined_risk = np.sqrt(combined_variance)
            scenarios[f"correlation_{corr}"] = combined_risk

        return {
            "correlation_scenarios": scenarios,
            "current_correlation": correlation,
            "risk_reduction_potential": scenarios["correlation_-0.5"] - scenarios["correlation_1.0"],
            "diversification_effectiveness": "High" if correlation < 0.3 else "Moderate" if correlation < 0.7 else "Low"
        }


class ActiveManagement:
    """Main active portfolio management interface"""

    def __init__(self):
        self.value_measurement = ValueAddedMeasurement()
        self.ir_analysis = InformationRatioAnalysis()
        self.fundamental_law = FundamentalLawActiveManagement()
        self.multifactor_models = MultifactorModels()
        self.active_risk_tracking = ActiveRiskTracking()
        self.strategy_combination = StrategyCombination()

    def comprehensive_active_management_analysis(self, portfolio_data: Dict,
                                                 benchmark_data: Dict,
                                                 factor_data: Optional[Dict] = None) -> Dict:
        """Comprehensive active management analysis"""

        portfolio_returns = np.array(portfolio_data.get("returns", []))
        benchmark_returns = np.array(benchmark_data.get("returns", []))

        # Value added measurement
        value_added = self.value_measurement.calculate_value_added(
            portfolio_returns, benchmark_returns
        )

        # Information ratio analysis
        ir_analysis = self.ir_analysis.calculate_information_ratio_ex_post(
            portfolio_returns, benchmark_returns
        )

        # Fundamental law analysis
        if "fundamental_law_inputs" in portfolio_data:
            fl_analysis = self.fundamental_law.calculate_fundamental_law_components(
                portfolio_data["fundamental_law_inputs"]
            )
        else:
            fl_analysis = {"note": "Fundamental law inputs not provided"}

        # Risk decomposition if factor data available
        risk_analysis = {}
        if factor_data:
            portfolio_weights = np.array(portfolio_data.get("weights", []))
            benchmark_weights = np.array(benchmark_data.get("weights", []))

            if len(portfolio_weights) == len(benchmark_weights):
                risk_analysis = self.active_risk_tracking.decompose_active_risk(
                    portfolio_weights,
                    benchmark_weights,
                    factor_data.get("factor_exposures", np.array([])),
                    factor_data.get("factor_covariance", np.array([])),
                    factor_data.get("specific_risk", np.array([]))
                )

        # Tracking risk analysis
        tracking_analysis = self.active_risk_tracking.calculate_tracking_risk(
            portfolio_returns, benchmark_returns
        )

        return {
            "value_added_analysis": value_added,
            "information_ratio_analysis": ir_analysis,
            "fundamental_law_analysis": fl_analysis,
            "risk_decomposition": risk_analysis,
            "tracking_risk_analysis": tracking_analysis,
            "active_management_assessment": self._assess_active_management_quality(
                value_added, ir_analysis, tracking_analysis
            ),
            "improvement_recommendations": self._generate_improvement_recommendations(
                value_added, ir_analysis, fl_analysis
            )
        }

    def manager_selection_analysis(self, manager_candidates: List[Dict]) -> Dict:
        """Analyze and compare active manager candidates"""

        manager_scores = {}

        for i, manager in enumerate(manager_candidates):
            manager_id = manager.get("manager_id", f"Manager_{i + 1}")

            # Calculate key metrics
            returns = np.array(manager.get("returns", []))
            benchmark_returns = np.array(manager.get("benchmark_returns", []))

            if len(returns) > 0 and len(benchmark_returns) > 0:
                ir_analysis = self.ir_analysis.calculate_information_ratio_ex_post(
                    returns, benchmark_returns
                )

                # Manager scoring
                score = self._calculate_manager_score(ir_analysis, manager)

                manager_scores[manager_id] = {
                    "information_ratio": ir_analysis["information_ratio_annualized"],
                    "tracking_error": ir_analysis["tracking_error_annualized"],
                    "active_return": ir_analysis["active_return_annualized"],
                    "overall_score": score,
                    "strengths": self._identify_manager_strengths(ir_analysis, manager),
                    "concerns": self._identify_manager_concerns(ir_analysis, manager)
                }

        # Rank managers
        ranked_managers = sorted(manager_scores.items(),
                                 key=lambda x: x[1]["overall_score"], reverse=True)

        return {
            "manager_analysis": manager_scores,
            "manager_ranking": [manager[0] for manager in ranked_managers],
            "top_manager": ranked_managers[0][0] if ranked_managers else None,
            "selection_criteria": self._define_manager_selection_criteria(),
            "due_diligence_framework": self._manager_due_diligence_framework()
        }

    def factor_model_implementation(self, factor_data: Dict) -> Dict:
        """Implement and analyze factor model for active management"""

        # APT analysis
        apt_analysis = self.multifactor_models.arbitrage_pricing_theory_analysis()

        # Factor model comparison
        model_comparison = self.multifactor_models.compare_factor_models()

        # Multiple risk dimensions
        risk_benefits = self.multifactor_models.multiple_risk_dimensions_benefits()

        # Practical applications
        model_uses = self.multifactor_models.analyze_multifactor_model_uses(factor_data)

        return {
            "apt_framework": apt_analysis,
            "factor_model_comparison": model_comparison,
            "multiple_risk_dimensions": risk_benefits,
            "practical_applications": model_uses,
            "implementation_recommendations": self._factor_model_implementation_recommendations(
                factor_data
            )
        }

    def _assess_active_management_quality(self, value_added: Dict,
                                          ir_analysis: Dict, tracking_analysis: Dict) -> Dict:
        """Assess overall active management quality"""

        ir = ir_analysis.get("information_ratio_annualized", 0)
        active_return = value_added.get("active_return_annualized", 0)
        hit_rate = value_added.get("hit_rate", 0.5)

        # Quality scoring
        quality_score = 0

        # Information ratio component (40%)
        if ir > 0.5:
            quality_score += 40
        elif ir > 0.25:
            quality_score += 30
        elif ir > 0:
            quality_score += 20

        # Active return component (30%)
        if active_return > 0.02:
            quality_score += 30
        elif active_return > 0:
            quality_score += 20

        # Hit rate component (20%)
        if hit_rate > 0.6:
            quality_score += 20
        elif hit_rate > 0.5:
            quality_score += 15

        # Consistency component (10%)
        te_stability = tracking_analysis.get("tracking_risk_stability", {}).get("stability_level", "Low")
        if te_stability == "High":
            quality_score += 10
        elif te_stability == "Moderate":
            quality_score += 5

        return {
            "quality_score": quality_score,
            "quality_rating": "Excellent" if quality_score > 80 else
            "Good" if quality_score > 60 else
            "Average" if quality_score > 40 else "Poor",
            "key_strengths": self._identify_key_strengths(ir, active_return, hit_rate),
            "areas_for_improvement": self._identify_improvement_areas(ir, active_return, hit_rate)
        }

    def _generate_improvement_recommendations(self, value_added: Dict,
                                              ir_analysis: Dict, fl_analysis: Dict) -> List[str]:
        """Generate specific improvement recommendations"""

        recommendations = []

        ir = ir_analysis.get("information_ratio_annualized", 0)
        if ir < 0.25:
            recommendations.append("Focus on improving information ratio through better forecasting or risk control")

        hit_rate = value_added.get("hit_rate", 0.5)
        if hit_rate < 0.55:
            recommendations.append("Improve hit rate through better market timing or security selection")

        if "fundamental_law_inputs" in fl_analysis:
            fl_inputs = fl_analysis.get("fundamental_law_inputs", {})
            ic = fl_inputs.get("information_coefficient", 0)
            breadth = fl_inputs.get("breadth", 0)
            tc = fl_inputs.get("transfer_coefficient", 0)

            if ic < 0.05:
                recommendations.append("Enhance forecasting skill (information coefficient)")
            if breadth < 50:
                recommendations.append("Increase breadth of investment opportunities")
            if tc < 0.7:
                recommendations.append("Improve implementation efficiency (transfer coefficient)")

        if not recommendations:
            recommendations.append("Continue current strategy while monitoring for performance persistence")

        return recommendations

    def _calculate_manager_score(self, ir_analysis: Dict, manager_data: Dict) -> float:
        """Calculate overall manager score"""

        ir = ir_analysis.get("information_ratio_annualized", 0)
        track_record_length = manager_data.get("track_record_years", 3)
        assets_under_management = manager_data.get("aum_billions", 1)

        # Base score from IR (60%)
        base_score = min(100, max(0, ir * 100)) * 0.6

        # Track record adjustment (25%)
        track_record_score = min(25, track_record_length * 5)

        # AUM adjustment (15%) - preference for moderate size
        if 1 <= assets_under_management <= 10:
            aum_score = 15
        elif 0.5 <= assets_under_management <= 20:
            aum_score = 10
        else:
            aum_score = 5

        return base_score + track_record_score + aum_score

    def _identify_manager_strengths(self, ir_analysis: Dict, manager_data: Dict) -> List[str]:
        """Identify manager strengths"""

        strengths = []

        ir = ir_analysis.get("information_ratio_annualized", 0)
        if ir > 0.5:
            strengths.append("Excellent risk-adjusted performance")

        active_return = ir_analysis.get("active_return_annualized", 0)
        if active_return > 0.02:
            strengths.append("Strong active return generation")

        track_record = manager_data.get("track_record_years", 3)
        if track_record > 5:
            strengths.append("Long track record demonstrates consistency")

        return strengths

    def _factor_model_implementation_recommendations(self, factor_data: Dict) -> List[str]:
        """Provide factor model implementation recommendations"""

        recommendations = [
            "Start with fundamental factor model for intuitive factor interpretation",
            "Ensure factor data quality and regular updates",
            "Implement robust factor exposure monitoring",
            "Use factor models for risk budgeting and portfolio construction",
            "Regular model validation and performance testing",
            "Consider transaction costs in factor-based strategies"
        ]

        return recommendations

    # Helper methods for various analyses
    def _identify_key_strengths(self, ir: float, active_return: float, hit_rate: float) -> List[str]:
        """Identify key strengths of active management"""
        strengths = []

        if ir > 0.5:
            strengths.append("Excellent information ratio")
        if active_return > 0.015:
            strengths.append("Strong active return generation")
        if hit_rate > 0.6:
            strengths.append("High hit rate indicates good timing")

        return strengths

    def _identify_improvement_areas(self, ir: float, active_return: float, hit_rate: float) -> List[str]:
        """Identify areas for improvement"""
        areas = []

        if ir < 0.25:
            areas.append("Information ratio below industry average")
        if active_return < 0.005:
            areas.append("Limited active return generation")
        if hit_rate < 0.5:
            areas.append("Hit rate below 50% indicates poor timing")

        return areas

    def _define_manager_selection_criteria(self) -> Dict:
        """Define criteria for manager selection"""
        return {
            "quantitative_criteria": [
                "Information ratio > 0.5",
                "Track record > 3 years",
                "Consistent performance across periods",
                "Reasonable fees relative to value added"
            ],
            "qualitative_criteria": [
                "Clear investment philosophy",
                "Experienced investment team",
                "Robust risk management process",
                "Transparent reporting and communication"
            ],
            "risk_criteria": [
                "Appropriate risk controls",
                "Style consistency",
                "Capacity constraints awareness",
                "Business risk assessment"
            ]
        }

    def _manager_due_diligence_framework(self) -> Dict:
        """Comprehensive manager due diligence framework"""
        return {
            "investment_process": [
                "Investment philosophy and approach",
                "Research process and resources",
                "Portfolio construction methodology",
                "Risk management framework"
            ],
            "performance_analysis": [
                "Return attribution analysis",
                "Risk-adjusted performance metrics",
                "Performance consistency",
                "Benchmark relative analysis"
            ],
            "organizational_assessment": [
                "Team experience and stability",
                "Business ownership structure",
                "Operational infrastructure",
                "Compliance and risk controls"
            ],
            "ongoing_monitoring": [
                "Regular performance review",
                "Process consistency verification",
                "Key person risk monitoring",
                "Capacity utilization tracking"
            ]
        }